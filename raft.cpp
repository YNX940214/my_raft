#include <iostream>
#include <vector>
#include <string>
#include <boost/asio.hpp>
#include "util.h"
#include <iostream>
#include "log/boost_lop.h"
#include "rpc/rpc.h"
#include "rpc.pb.h"
#include "entry/entry.h"

using namespace boost::asio;
using namespace std;
using boost::asio::ip::tcp;
using namespace raft_rpc;
enum State {
    follower, candidate, primary
};

//3. 先把程序跑起来，做出第一次选举。
//4。根据commitIndex把entry apply到state machine的独立线程，注意这个线程只能读commitIndex不能改
class instance {
public:
    instance(io_service &loop, const string &_ip, int _port, const tcp::endpoint &_endpoint) :
            ip_(_ip),
            port_(_port),
            server_(_ip, _port),
            ioContext(loop),
            term_(0),
            state_(follower),
            already_voted_(false),
            candidate_timer_(loop),
            network_(loop, _endpoint, std::bind(&instance::reactToIncomingMsg, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)),
//            state_machine_(),
            winned_votes_(0) {
        load_config_from_file();
        for (const auto &server_tuple: configuration_) {
            nextIndex_[server_tuple] = -1; // -1 not 0, because at the beginning, entries[0] raise Exception,
            Follower_saved_index[server_tuple] = 0;
            current_rpc_lsn_[server_tuple] = 0;
            auto sp = make_shared<deadline_timer>(loop);
            retry_timers_.insert({server_tuple, sp});
        }
    }

    //mock 一下先
    void load_config_from_file() {
        configuration_ = {
                {"127.0.0.1", 8888},
                {"127.0.0.1", 7777},
                {"127.0.0.1", 9999},
        };
        majority_ = 2;
        N_ = 3;
    }

    void run() {
        network_.startAccept();
        int waiting_counts = candidate_timer_.expires_from_now(boost::posix_time::milliseconds(random_candidate_expire()));
        if (waiting_counts != 0) {
            throw std::logic_error("when server starts this can't be 0");
        } else {
            candidate_timer_.async_wait([this](const boost::system::error_code &error) {
                Log_trace << "handler in run() expired, error: " << error.message();
                if (error == boost::asio::error::operation_aborted) {
                    Log_trace << "handler in run() canceled()";
                    //可能会被在follow_deal_VoteRPC中取消，或者ARPC中取消，但是按照目前看来被cancel什么都不用管，把执行流让给主动cancel定时器的函数就行了
                } else {
                    trans2C();
                }
            });
        }
        ioContext.run();
    }

    void cancel_all_timers() {
        Log_trace << "begin";
        int canceled_number = candidate_timer_.cancel();
        {
            std::ostringstream oss;
            oss << canceled_number << " handlers was canceled on the candidate_timer_" << endl;
            string s = oss.str();
            Log_debug << s;
        }

        /*
         * since cancel function can not cancel the handlers that is already expired, see boost_tiemr_bug(2).cpp, we reference
         *  https://stackoverflow.com/questions/43168199/cancelling-boost-asio-deadline-timer-safely
         *  to cancel the expired handlers by setting its timer's expire time at a special value,
         *  and when executed the handlers will check if the special value is set, if set, that means the handler has been canceled.
         */
        for (auto pair : retry_timers_) {
            pair.second->expires_at(boost::posix_time::min_date_time);
        }
        Log_trace << "done";
    }

private:
    void trans2F(int term) {
        Log_info << "begin";
        cancel_all_timers();
        winned_votes_ = 0;
        term_ = term;
        state_ = follower;
        int waiting_count = candidate_timer_.expires_from_now(boost::posix_time::milliseconds(random_candidate_expire()));
        if (waiting_count != 0) {
            throw std::logic_error("trans2F在一开始已经执行了cancel_all_timers函数，所以这里不应该有waiting_count=0");
        }
        candidate_timer_.async_wait([this](const boost::system::error_code &error) {
            Log_trace << "handler in trans2F expired, error: " << error.message();
            if (error == boost::asio::error::operation_aborted) {
                Log_debug << "handler in trans2F is canceled";
            } else {
                trans2C();
            }
        });
        Log_info << "done";
    }

    void trans2P() {
        Log_info << "begin";
        cancel_all_timers();
        state_ = primary;
        for (const auto &server: configuration_) {
            AE(server);
        }
        Log_info << "done";
    }

    void trans2C() {
        Log_info << "begin";
        /*
         * candidate can trans 2 candidate, we need to cancel the timer
         * 刚跑起来的时候这里有个bug，rv的重传超时设置成了秒，这导致trans2candidate的时候，rv的timer还没有超时，所以再次set timer expire的时候检查hook数目不为0，属于一个被遗漏的bug，但找到后修复了，但是这
         */
        cancel_all_timers();
        term_++;
        Log_info << "current term: " << term_;
        state_ = candidate;
        winned_votes_ = 1;
        //设置下一个term candidate的定时器
        int waiting_count = candidate_timer_.expires_from_now(boost::posix_time::milliseconds(random_candidate_expire()));
        if (waiting_count == 0) {
            //完全可能出现这种情况，如果是cb_trans2_candidate的定时器自然到期，waiting_count就是0
            //leave behind
        } else {
            throw std::logic_error("trans2C只能是timer_candidate_expire自然到期触发，所以不可能有waiting_count不为0的情况，否则就是未考虑的情况");
        }
        candidate_timer_.async_wait([this](const boost::system::error_code &error) {
            Log_trace << "handler in trans2C expired, error: " << error.message();
            if (error == boost::asio::error::operation_aborted) {
                //do nothing, leave react to deal with the logic
            } else {
                trans2C();
            }
        });
        for (auto &server : configuration_) {
            int port = std::get<1>(server);
            if (port != port_) {
                RV(server);
            }
        }
        Log_info << "done";
    }

private:
    string build_rpc_ae_string(const tuple<string, int> &server) {
        Log_trace << "begin: server: " << server;
        AppendEntryRpc rpc;
        unsigned int server_current_rpc_lsn = current_rpc_lsn_[server];
        rpc.set_lsn(server_current_rpc_lsn);
        rpc.set_term(term_);
        rpc.set_commit_index(commit_index_);

        int index_to_send = nextIndex_[server];
        if (index_to_send > 0) {
            rpc_Entry prev_entry = entries_.get(index_to_send - 1);
            rpc.set_prelog_term(prev_entry.term());
        } else if (index_to_send <= 0) {
            rpc.set_prelog_term(-1);
        }

        if (index_to_send == entries_.size()) {
            //empty rpc as HB
        } else {
            const rpc_Entry &entry = entries_.get(index_to_send);
            rpc_Entry *entry_to_send = rpc.add_entry();
            entry_to_send->set_term(entry.term());
            entry_to_send->set_msg(entry.msg());
            entry_to_send->set_index(entry.index());
        }
        string msg;
        rpc.SerializeToString(&msg);
        return to_string(APPEND_ENTRY) + msg;
    }

    void AE(const tuple<string, int> &server) {
        Log_debug << "begin";
        string ae_rpc_str = build_rpc_ae_string(server);
        writeTo(server, ae_rpc_str);   //考虑如下场景，发了AE1,index为100,然后定时器到期重发了AE2,index为100，这时候收到ae1的resp为false，将index--,如果ae2到F，又触发了一次resp，这两个rpc的请求一样，resp一样，但是P收到两个一样的resp会将index--两次
        shared_ptr<deadline_timer> t1 = retry_timers_[server];
        int waiting_counts = t1->expires_from_now(boost::posix_time::milliseconds(random_ae_retry_expire()));
        if (waiting_counts != 0) {
            throw std::logic_error("should be zero, a timer can't be interrupt by network, but when AE is called, there should be no hooks on the timer");
        } else {
            t1->async_wait([this, server](const boost::system::error_code &error) {
                if (error == boost::asio::error::operation_aborted) {
                    //do nothing, maybe log
                } else {
                    /* situtations:
                     * One: if the ae is lost, AE(server) will get the same index to send to followers
                     * Two: P's index is [0,1], F's index is [0], P send ae(index=1) to F, resp_ae returns, the ok is true, the primary found (currentIndex[server]+1 == entries_.size()), then P won't cancel the timer but only add the currentIndex to 2, when the timer expires, it can found 2==entries_.size(), so send an empty AE
                     * Three: P's index is [0,1,2], F's index is [0], P send ae(index=1) to F, resp_ae returns, the ok is true, the primary found (currentIndex[server]+1 "that's 2" < entries_.size() "that's 3" ), so P cancel the timer, add the currentIndex, and immediately send the next AE*/
                    AE(server); //this will re-get the index to send, if before the timer expires, the resp returns and ok, and the resp is the ae of the last index, then we will not
                }
            });
        }
        BOOST_LOG_TRIVIAL(trace) << "[done] AE ";
    }

    string build_rpc_rv_string(const tuple<string, int> &server) {
        BOOST_LOG_TRIVIAL(trace) << "[begin] build_rpc_rv_string ";

        unsigned int server_current_rpc_lsn = current_rpc_lsn_[server];
        RequestVoteRpc rpc;
        rpc.set_lsn(server_current_rpc_lsn);
        rpc.set_term(term_);
        rpc.set_latest_index(entries_.size() - 1);
        string msg;
        rpc.SerializeToString(&msg);

        BOOST_LOG_TRIVIAL(trace) << "[begin] build_rpc_rv_string ";
        return to_string(REQUEST_VOTE) + msg;
    }

    void RV(const tuple<string, int> &server) {
        BOOST_LOG_TRIVIAL(trace) << "[begin] RV: " << server2str(server);
        string rv_rpc_str = build_rpc_rv_string(server);
        writeTo(server, rv_rpc_str);
        shared_ptr<deadline_timer> timer = retry_timers_[server];
        auto last_expire_time = timer->expires_at();
        int waiting_counts = timer->expires_from_now(boost::posix_time::milliseconds(random_rv_retry_expire()));
        if (last_expire_time == boost::posix_time::min_date_time) {
            //canell_all_timers is called, so the retry should be canceled (no longer need to hook retry handler), just return
            BOOST_LOG_TRIVIAL(trace) << "RV handler is canceled by setting its timer's expiring time to min_date_time";
            return;
        }

        if (waiting_counts != 0) {
            throw logic_error("if the cancel_all_timers has already canceled the RV retry, the exe stream will not come to here, as it comes here, there is something wrong");
        } else {
            timer->async_wait([this, server](const boost::system::error_code &error) {
                BOOST_LOG_TRIVIAL(trace) << "[begin] RV timer expired handler: " << server2str(server) << ", error: " << error.message();
                if (error == boost::asio::error::operation_aborted) {
                    throw logic_error("其实我们不会运行到这里了，因为已经通过 set timer的方式设置定时器超时");
                } else {
                    RV(server);
                }
            });
        }
        BOOST_LOG_TRIVIAL(trace) << "[done] RV: " << server2str(server);
    }

private:
    void reactToIncomingMsg(RPC_TYPE rpc_type_, const string msg, const tuple<string, int> &server) {
        Log_trace << "begin: RPC_TYPE: " << rpc_type_ << ", msg: " << msg << ", server: " << server2str(server);
        //proto buf decode
        if (rpc_type_ == REQUEST_VOTE) {
            RequestVoteRpc rv;
            rv.ParseFromString(msg);
            react2rv(server, rv);
        } else if (rpc_type_ == APPEND_ENTRY) {
            AppendEntryRpc ae;
            ae.ParseFromString(msg);
            react2ae(server, ae);
        } else if (rpc_type_ == RESP_VOTE) {
            Resp_RequestVoteRpc resp_rv;
            resp_rv.ParseFromString(msg);
            react2resp_rv(server, resp_rv);
        } else if (rpc_type_ == RESP_APPEND) {
            Resp_AppendEntryRpc resp_ae;
            resp_ae.ParseFromString(msg);
            react2resp_ae(server, resp_ae);
        } else {
            BOOST_LOG_TRIVIAL(error) << "unknown action: " << rpc_type_ << ", the whole msg string is:\n" + msg;
        }
        BOOST_LOG_TRIVIAL(trace) << "[done] reactToIncomingMsg( RPC_TYPE:  " << rpc_type_ << ", msg: " << msg << ", server: " << server2str(server);
    }

    void react2ae(const tuple<string, int> &server, AppendEntryRpc rpc_ae) {
        BOOST_LOG_TRIVIAL(trace) << "[begin] react2ae(server: " << server2str(server);

        int remote_term = rpc_ae.term();
        int prelog_term = rpc_ae.prelog_term();
        int commit_index = rpc_ae.commit_index();
        int rpc_lsn = rpc_ae.lsn();

        // 这行错了!       assert(commitIndex <= prelog_index); 完全有可能出现commitIndex比prelog_index大的情况
        if (term_ > remote_term) {
            make_resp_ae(false, "react2ae, term_ > term", server, rpc_lsn);
            return;
        } else { //term_ <= remote_term
            if (state_ == primary && term_ == remote_term) {
                throw std::logic_error("error, one term has two primaries");
            }
            trans2F(remote_term);//会设置定时器，所以此函数不需要设置定时器了
            /*
             一开始想法是commitIndex直接赋值给本地commitIndex，然后本地有一个线程直接根据本第commitIndex闷头apply to stateMachine就行了（因为两者完全正交），
             但是一想不对，因为rpc的commitIndex可能比prelog_index大，假设本地和主同步的entry是0到20，21到100都是stale的entry，rpc传来一个commitIndex为50，prelog_index为80（还没走到真确的20），
             难道那个线程就无脑把到80的entry给apply了？所以本地的commitIndex只能是
             */
            int rpc_ae_entry_size = rpc_ae.entry_size();
            if (rpc_ae_entry_size == 0) { // empty ae, means that this primary thinks this follower has already catch up with it
                make_resp_ae(true, "成功insert", server, rpc_lsn);
                return;
            } else if (rpc_ae_entry_size == 1) {
                const raft_rpc::rpc_Entry &entry = rpc_ae.entry(0);
                int prelog_index = entry.index() - 1;
                if (prelog_term == -1) { //see AE, we set prelog_term to -1 as this is the index0 log of primary, the follower has to accept it
                    entries_.insert(0, entry);
                    // we choose the smaller one to be the commit index, the logic is in entries_.update_commit_index function
                    follower_update_commit_index(commit_index, prelog_index);
                    make_resp_ae(true, "成功insert", server, rpc_lsn);
                    return;
                } else {
                    unsigned last_index = entries_.size() - 1;
                    if (last_index < prelog_index) {
                        make_resp_ae(false, "react2ae, term is ok, but follower's last entry index is " + to_string(last_index) + ", but prelog_index is " + to_string(prelog_index), server, rpc_lsn);
                        return;
                    } else { // if (last_index >= prelog_index)  注意> 和 = 的情况是一样的，（至少目前我是这么认为的）
                        int term_of_local_entry_with_prelog_index = entries_.get(prelog_index).term();
                        if (term_of_local_entry_with_prelog_index == prelog_term) {
                            entries_.insert(prelog_index + 1, entry);
                            // we choose the smaller one to be the commit index, the logic is in entries_.update_commit_index function
                            follower_update_commit_index(commit_index, prelog_index);
                            make_resp_ae(true, "成功insert", server, rpc_lsn);
                            return;
                        } else if (term_of_local_entry_with_prelog_index < prelog_term) {
                            // easy to make an example
                            make_resp_ae(false, "react2ae, term is ok, but follower's entry's term" + to_string(term_of_local_entry_with_prelog_index) + "of prelog_index " + to_string(prelog_index) + "is SMALLER than rpc's prelog_term" + to_string(prelog_term), server, rpc_lsn);
                            return;
                        } else {
                            make_resp_ae(false, "react2ae, term is ok, but follower's entry's term" + to_string(term_of_local_entry_with_prelog_index) + "of prelog_index " + to_string(prelog_index) + "is BIGGER than rpc's prelog_term" + to_string(prelog_term), server, rpc_lsn);
                            return;
                            /*
                             * 1. 5 nodes with term 1, index [2]
                             * 2. a trans P (term2), index[1 2], but copy to none
                             * 3. b trans P (term3), index[1,3], copy to c
                             * 4. a trans P (term4), and receive client, index [1,2,4], then copy to all, prelog_term is 2,prelog_index is 1,
                             * so for b and c, index 1 's term is 3, but remote prelog_term is 2, is completely ok.
                             */
                        }
                    }
                }
            } else {
                throw logic_error("rpc_ae's entry size is bigger 1");
            }
        }
        BOOST_LOG_TRIVIAL(trace) << "[done] react2ae(server: " << server2str(server);
    }

    //begin from here
    void react2rv(tuple<string, int> server, RequestVoteRpc rpc_rv) {
        Log_trace << "begin: server: " << server2str(server) << ", RV: ...";
        int remote_term = rpc_rv.term();
        int rpc_lsn = rpc_rv.lsn();
        int remote_latest_term = rpc_rv.latest_term();
        int remote_latest_index = rpc_rv.latest_index();

        if (term_ > remote_term) {
            make_resp_rv(false, "react2rv: term_ > remote,  write back false", server, rpc_lsn);
            return;
        } else if (term_ < remote_term) {
            trans2F(remote_term);
            //do not return, do logic
        }
        if (state_ == candidate || state_ == primary) {
            make_resp_rv(false, "react2rv: term_ = remote,  write back false", server, rpc_lsn);
        } else {
            if (!already_voted_) {  //这个if针对 term_ == term 的情况，只有一票
                bool should_vote = false;
                if (entries_.size() == 0) { //只管投票
                    should_vote = true;
                } else {
                    const rpc_Entry &lastEntry = entries_.get(entries_.size() - 1);
                    if (lastEntry.term() > remote_term) {
                        should_vote = false;
                    } else if (lastEntry.term() < remote_term) {
                        should_vote = true;
                    } else {
                        if (lastEntry.index() > remote_latest_index) { //todo deep think，why this will work，然后我就吃饭去了
                            should_vote = false;
                        } else {
                            should_vote = true;
                        }
                    }
                }
                if (should_vote) {
                    make_resp_rv(true, "react2rv: A vote is send", server, rpc_lsn);
                    already_voted_ = true;
                } else {
                    make_resp_rv(false, "react2rv: A_vote is denied for primary doesn't have newer log", server, rpc_lsn);
                }
            } else {
                make_resp_rv(false, "react2rv: No votes left this term", server, rpc_lsn);
            }
        }
        Log_trace << "done: server: " << server2str(server) << ", RV: ...";
    }

    void react2resp_ae(const tuple<string, int> &server, Resp_AppendEntryRpc &resp_ae) {
        BOOST_LOG_TRIVIAL(trace) << "[begin] react2resp_ae(server: " << server2str(server);

        int remote_term = resp_ae.term();
        if (term_ < remote_term) {
            trans2F(remote_term);
        } else if (term_ > remote_term) {
            //do nothing, even primary with the correct rpc_lsn, can't do anything for safety, not even response
        } else {
            if (state_ == primary) {
                primary_deal_resp_ae_with_same_term(server, resp_ae);
            } else {
                throw std::logic_error("不可能的情况，主ae，收到resp_ae的remote_term与term_相等，但是state不为主，说明一个term出现了两个主，错误");
            }
        }
        BOOST_LOG_TRIVIAL(trace) << "[done] react2resp_ae(server: " << server2str(server);

    }

    void react2resp_rv(tuple<string, int> server, Resp_RequestVoteRpc &resp_rv) {
        BOOST_LOG_TRIVIAL(trace) << "[begin] react2resp_rv(server: " << server2str(server);

        int remote_term = resp_rv.term();
        if (term_ < remote_term) {
            trans2F(remote_term);
        } else if (term_ > remote_term) {
            //do nothing
        } else {
            if (state_ == candidate) {
                candidate_deal_resp_rv_with_same_term(server, resp_rv);
            }
        }
        BOOST_LOG_TRIVIAL(trace) << "[done] react2resp_rv(server: " << server2str(server);

    }

private:
    void make_resp_rv(bool vote, string context_log, tuple<string, int> server, int lsn) {
        BOOST_LOG_TRIVIAL(trace) << "[begin] make_resp_rv(vote: " << vote << ", context_log: " << context_log << ", server: " << server2str(server) << "lsn: " << lsn << ")";
        Resp_RequestVoteRpc resp_vote;
        resp_vote.set_ok(vote);
        resp_vote.set_term(term_);
        resp_vote.set_lsn(lsn);
        string msg;
        resp_vote.SerializeToString(&msg);
        writeTo(server, to_string(RESP_VOTE) + msg);
        BOOST_LOG_TRIVIAL(trace) << "[done] make_resp_rv(vote: " << vote << ", context_log: " << context_log << ", server: " << server2str(server) << "lsn: " << lsn << ")";
    }

    void make_resp_ae(bool ok, string context_log, tuple<string, int> server, int lsn) {
        BOOST_LOG_TRIVIAL(trace) << "[begin] make_resp_ae(ok: " << ok << ", context_log: " << context_log << ", server: " << server2str(server) << "lsn: " << lsn << ")";
        Resp_AppendEntryRpc resp_entry;
        resp_entry.set_ok(ok);
        resp_entry.set_term(term_);
        resp_entry.set_lsn(lsn);
        string msg;
        resp_entry.SerializeToString(&msg);
        writeTo(server, to_string(RESP_APPEND) + msg);
        BOOST_LOG_TRIVIAL(trace) << "[done] make_resp_ae(ok: " << ok << ", context_log: " << context_log << ", server: " << server2str(server) << "lsn: " << lsn << ")";
    }

private: //helper functions, react2rv and react2ae is very simple in some situations (like term_ < remote), but some situations really wants deal(like for resp2rv a vote is granted), we deal them here
    // this is term_ == remote term, otherwise we deal somewhere else(actually just deny or trans2F)
    inline void primary_deal_resp_ae_with_same_term(const tuple<string, int> &server, Resp_AppendEntryRpc &resp_ae) {
        BOOST_LOG_TRIVIAL(trace) << "[begin] primary_deal_resp_ae_with_same_term(server: " << server2str(server);

        int resp_lsn = resp_ae.lsn();
        int current_rpc_lsn = current_rpc_lsn_[server];
        if (current_rpc_lsn != resp_lsn) {
            return;
        } else {
            bool ok = resp_ae.ok();
            if (ok) {
                int last_send_index = nextIndex_[server];
                if (last_send_index > entries_.size()) {
//                    don't nextIndex_[server] = last_send_index + 1;
                } else {
                    nextIndex_[server] = last_send_index + 1;
                }


                // whether send next entry immediately is determined by primary_deal_resp_ae, but whether to build an empty ae is determinde by AE()
                if (last_send_index > entries_.size()) {
                    // > , we don't send next index immediately, let the timers expires and send empty rps_ae as heartbeat, that situation 2 in AE()
                } else {
                    primary_update_commit_index(server, last_send_index);
                    if (last_send_index == entries_.size()) {
                        // = , we don't send next index immediately, let the timers expires and send empty rps_ae as heartbeat, that situation 2 in AE()
                    } else {
                        //cancel timer todo check hook num
                        retry_timers_[server]->cancel();
                        // immediately the next AE
                        AE(server);
                    }
                }
            } else {
                /* ok == false, but there may be many possibilities, like remote server encouter disk error, what should it do(one way is just crash, no response, so primary will not push back the index to send)
                 * so we must ensure !!!!!!!!!!!!! the remote return resp_ae( ok =false ) only in one situation, that prev_index != rpc_index -1, any other (disk failure, divide 0) will nerver return false! just crash it.
                 */
                int last_send_index = nextIndex_[server];
                if (last_send_index <= 0) {
                    // don't nextIndex_[server] = last_send_index - 1;
                } else {
                    nextIndex_[server] = last_send_index - 1;
                }
                if (last_send_index <= 0) {
                    // do nothing, let the timers expires
                } else {
                    retry_timers_[server]->cancel();
                    AE(server);
                }

            }
        }
        BOOST_LOG_TRIVIAL(trace) << "[done] primary_deal_resp_ae_with_same_term(server: " << server2str(server);

    }

    // candidate deal with resp_rv with the same term (because other resp_rv situation is simple, only this one needs a little logic)
    inline void candidate_deal_resp_rv_with_same_term(const tuple<string, int> &server, Resp_RequestVoteRpc &resp_rv) {
        BOOST_LOG_TRIVIAL(trace) << "[begin] candidate_deal_resp_rv_with_same_term(server: " << server2str(server);

        int resp_lsn = resp_rv.lsn();
        int current_rpc_lsn = current_rpc_lsn_[server];
        if (current_rpc_lsn != resp_lsn) {
            return;
        } else {
            bool ok = resp_rv.ok();
            if (ok) {
                winned_votes_++;
                if (winned_votes_ > needed_votes_) {
                    trans2P();
                } else {
                    // cancel the timer, if receive votes, we no longer need to rv more this term
                    shared_ptr<deadline_timer> timer = retry_timers_[server];
                    timer->cancel();
                }
            } else {
                // this mean remote follower has already voted for other candidate with the same term, just do nothing and let candidate_timer expires to enter the next term candidate
            }
        }
        BOOST_LOG_TRIVIAL(trace) << "[done] candidate_deal_resp_rv_with_same_term(server: " << server2str(server);

    }

private:
    inline void primary_update_commit_index(const tuple<string, int> &server, int index) {
        BOOST_LOG_TRIVIAL(trace) << "[begin] primary_update_commit_index(server: " << server2str(server) << ", index: " << index << ")";

        Follower_saved_index[server] = index;
        // this can be done by find the k
        vector<int> temp_sort(configuration_.size(), 0);
        int i = 0;
        for (const auto &ele:Follower_saved_index) {
            temp_sort[i] = ele.second;
        }
        std::reverse(temp_sort.begin(), temp_sort.end());
        commit_index_ = temp_sort[majority_];
        BOOST_LOG_TRIVIAL(trace) << "[done] primary_update_commit_index(server: " << server2str(server) << ", index: " << index << ")";

    }

    inline void follower_update_commit_index(unsigned remote_commit_index, unsigned remote_prelog_index) {
        BOOST_LOG_TRIVIAL(trace) << "[begin] follower_update_commit_index(remote_commit_index: " << remote_commit_index << ", remote_prelog_index: " << remote_prelog_index << ")";

        int smaller_index = smaller(remote_commit_index, remote_prelog_index);
        if (smaller_index > commit_index_) {
            commit_index_ = smaller_index;
        } else if (smaller_index == commit_index_) {

        } else {
            string err = "remote_commit_index " + std::to_string(smaller_index) + " is smaller than local commit_index " + std::to_string(commit_index_) + ", this is impossible";
            BOOST_LOG_TRIVIAL(error) << err;
        }
        BOOST_LOG_TRIVIAL(trace) << "[done] follower_update_commit_index(remote_commit_index: " << remote_commit_index << ", remote_prelog_index: " << remote_prelog_index << ")";

    }

    void writeTo(tuple<string, int> server, string msg) {
//        在这里写个函数将msg print解析一下看看是不是真确的 todo
//        Log_trace << "begin: server: "<<server2str(server)<<", "

        network_.writeTo(server, msg, std::bind(&instance::deal_with_write_error, this, std::placeholders::_1, std::placeholders::_2));
        BOOST_LOG_TRIVIAL(trace) << "[done] writeTo(server: " << server2str(server) << ", msg: " << msg << ")";

    }

    void deal_with_write_error(boost::system::error_code &ec, std::size_t) {
        //todo consider what to do when write failed, (maybe not nothing, because a write failed may means network unreachable, let the retry mechanism to deal with it)
        BOOST_LOG_TRIVIAL(error) << "write error" << ec.message();

    }

private:
    int winned_votes_;
    int needed_votes_;
    //可以设置为bool，因为raft规定一个term只能一个主, 考虑这种情况，candidate发出了rv，但是所有f只能接受rv，不能发会resp_rv,但是我的设定中f在收到相同term的rv时不会重传，因为c会自动增加term再rv，只有收到了更高term的rv，f才会resp，所以这里只用bool就够了
    bool already_voted_;

    //normal
    int term_;
    State state_;
    io_context &ioContext;
    vector<std::tuple<string, int>> configuration_;
    int majority_;
    int N_;
    //rpc
    tcp::endpoint endpoint_;
    string ip_;
    int port_;
    std::tuple<string, int> server_;
    map<std::tuple<string, int>, int> current_rpc_lsn_; //if resp_rpc's lsn != current[server]; then just ignore; (send back the index can't not ensuring idempotence; for example we received a resp which wandereding in the network for 100 years)
    RPC network_;

    //timers
    deadline_timer candidate_timer_;
    map<std::tuple<string, int>, shared_ptr<deadline_timer> > retry_timers_;

    // log & state machine
    int commit_index_;
    Entries entries_;
    map<tuple<string, int>, int> nextIndex_;
    map<tuple<string, int>, int> Follower_saved_index;
//    StateMachine state_machine_; todo
};

int main(int argc, char **argv) {
    try {
        int _port = atoi(argv[1]);
        init_logging(_port);
        boost::asio::io_service io;
        tcp::endpoint _endpoint(tcp::v4(), _port);
        instance raft_instance(io, "127.0.0.1", _port, _endpoint);
        raft_instance.run();
    } catch (std::exception &exception) {
        BOOST_LOG_TRIVIAL(error) << " exception: " << exception.what();
    }
    return 0;
}