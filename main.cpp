#include <iostream>
#include <vector>
#include <string>
#include <boost/asio.hpp>
#include "util.h"
#include <iostream>
#include "log/boost_lop.h"
#include "rpc/rpc.h"
#include "rpc.pb.h"
#include "entry.h"
#include "const.h"

using namespace boost::asio;
using namespace std;
using boost::asio::ip::tcp;
using namespace raft_rpc;
enum State {
    follower, candidate, primary
};


//-1。 目前最当务之急的问题是弄清楚如何rpc，我现在是sendback这样操作的（这是最自然的，这个sendback是对触发这个函数的rpc的sendback，如何抽象是个问题）
//并考虑如下例子中的场景，a 发出vote rpc， b收到，发回， 回到前不久a收到qurom的vote，变身成主，如何用rpc表示。思考sendback除了ok和term是否还要返回别的信息
//
//2. entry是个class，操作entry会写入磁盘，是一个对文件的封装
//3. 先把程序跑起来，做出第一次选举。
//4。根据commitIndex把entry apply到state machine的独立线程，注意这个线程只能读commitIndex不能改
class instance {
public:
    instance(io_service &loop, string _ip, int _port) :
            ip_(_ip),
            port_(_port),
            ioContext(loop),
            term_(0),
            state_(follower),
            F_already_voted(false),
            timer_candidate_expire_(loop),
            winned_votes_(0),
            state_machine_(),
            network_(loop, std::bind(&instance::reactToIncomingMsg, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4)) {
        load_config_from_file();

        for (auto server_tuple: configuration_) {
            nextIndex_[server_tuple] = -1; // -1 not 0, because at the beginning, entries[0] raise Exception,
            Follower_saved_index[server_tuple] = 0;
            current_rpc_lsn_[server_tuple] = 0;
        }
    }

    void run() {
        network_.startAccept();
        int waiting_counts = timer_candidate_expire_.expires_from_now(boost::posix_time::seconds(random_candidate_expire()));
        if (waiting_counts == 0) {
            throw "初始化阶段这里不可能为0";
        } else {
            timer_candidate_expire_.async_wait([this](const boost::system::error_code &error) {
                if (error == boost::asio::error::operation_aborted) {
                    //可能会被在follow_deal_VoteRPC中取消，或者ARPC中取消，但是按照目前看来被cancel什么都不用管，把执行流让给主动cancel定时器的函数就行了
                } else {
                    trans2candidate();
                }
            });
        }
        ioContext.run();
    }

    void writeTo(tuple<string, int> server, string msg) {
        network_.writeTo(server, msg, std::bind);
    }

    void reactToIncomingMsg(RPC_TYPE rpc_type_, string msg, tuple<string, int> server) {
        //proto buf decode
        if (rpc_type_ == REQUEST_VOTE) {
            RequestVoteRpc voteRpc;
            voteRpc.ParseFromString(msg);
            react2_VoteRPC(server, voteRpc);
        } else if (rpc_type_ == APPEND_ENTRY) {
            AppendEntryRpc append_rpc;
            append_rpc.ParseFromString(msg);
            react2_AE(server, append_rpc);
        } else if (rpc_type_ == RESP_VOTE) {
            if (state_ == follower) {

            } else if (state_ == candidate) {

            } else {

            }
        } else if (rpc_type_ == RESP_APPEND) {
            if (state_ == follower) {
                //do nothing
            } else if (state_ == candidate) {
                //do nothing
            } else {
                primary_react2_resp_append_entry();
            }
        } else {
            BOOST_LOG_TRIVIAL(error) << "unknown action: " << rpc_type_ << ", the whole msg string is:\n" + msg;
        }
    }

    void trans2follower(int term) {
        //todo clean election state（从candidate trans过来）
        term_ = term;
        state_ = follower;
        int waiting_count = timer_candidate_expire_.expires_from_now(boost::posix_time::seconds(random_candidate_expire()));
        if (waiting_count == 0) {
            throw "trans2follower是因为收到了外界rpc切换成follower，所以至少有一个cb_tran2_candidate的定时器，否则就是未考虑的情况";
        }
        timer_candidate_expire_.async_wait([this](const boost::system::error_code &error) {
            if (error == boost::asio::error::operation_aborted) {
                //啥都不做，让react处理
            } else {
                trans2candidate();
            }
        });
    }

    void trans2candidate() {
        term_++;
        state_ = candidate;
        winned_votes_ = 1;
        //设置下一个term candidate的定时器
        int waiting_count = timer_candidate_expire_.expires_from_now(boost::posix_time::seconds(random_candidate_expire()));
        if (waiting_count == 0) {
            //完全可能出现这种情况，如果是cb_trans2_candidate的定时器自然到期，waiting_count就是0
            //leave behind
        } else {
            throw ("trans2candidate只能是timer_candidate_expire自然到期触发，所以不可能有waiting_count不为0的情况，否则就是未考虑的情况");
        }
        request_votes();
    }

    void resp_2_vote(bool vote, string context_log, tuple<string, int> server) {
        Resp_RequestVoteRpc resp_vote;
        resp_vote.set_ok(vote);
        resp_vote.set_term(term_);
        string msg;
        resp_vote.SerializeToString(&msg);
        network_.writeTo(server, msg, [context_log](boost::system::error_code &ec, std::size_t) {
            if (!ec) {
                BOOST_LOG_TRIVIAL(debug) << "callback of '" << context_log << "' done";
            } else {
                BOOST_LOG_TRIVIAL(info) << "callback of '" << context_log << "' got an  error: " << ec;
            }
        });
    }

    //term 没用
    void resp2entry(bool ok, string context_log, tuple<string, int> server) {
        Resp_AppendEntryRPC resp_entry;
        resp_entry.set_ok(ok);
        resp_entry.set_term(term_);
        string msg;
        resp_entry.SerializeToString(&msg);
        network_.writeTo(server, msg, [context_log](boost::system::error_code &ec, std::size_t) {
            if (!ec) {
                BOOST_LOG_TRIVIAL(debug) << "callback of '" << context_log << "' done";
            } else {
                BOOST_LOG_TRIVIAL(info) << "callback of '" << context_log << "' got an  error: " << ec;
            }
        });
    }


    //有张表对比三种情况，对于term>remote,和<remote的情况，三个state是相同的，只有=remote的情况才是不同的，所以这里根据term封装
    void react2_VoteRPC(tuple<string, int> server, RequestVoteRpc vote_rpc) {
        int term = vote_rpc.term();
        int vote_index = vote_rpc.index();
        if (term_ > term) {
            resp_2_vote(false, "react2_VoteRPC: term_ > remote,  write back false", server);
            return;
        } else if (term_ < term) {
            trans2follower(term);
            resp_2_vote(true, "react2_VoteRPC: term_ < remote,  write back true", server);
            return;
        } else {
            if (state_ == follower || state_ == primary) {
                resp_2_vote(false, "react2_VoteRPC: term_ = remote,  write back false", server);
            } else {
                if (!F_already_voted) {  //这个if针对 term_ == term 的情况，只有一票
                    bool should_vote = false;
                    if (entries_.size() == 0) { //只管投票
                        should_vote = true;
                    } else {
                        Entry &lastEntry = entries_.last_entry();
                        if (lastEntry.Term() > term) {
                            should_vote = false;
                        } else if (lastEntry.Term() < term) {
                            should_vote = true;
                        } else {
                            if (lastEntry.Index() > vote_index) {
                                should_vote = false;
                            } else {
                                should_vote = true;
                            }
                        }
                    }
                    if (should_vote) {
                        resp_2_vote(false, "react2_VoteRPC: A vote is send", server);
                    } else {
                        resp_2_vote(false, "react2_VoteRPC: A_vote is denied", server);
                    }
                } else {
                    resp_2_vote(false, "react2_VoteRPC: No votes left this term", server);
                }
            }
            return;
        }
    }


    void react2_resp_ae(tuple<string, int> server, Resp_AppendEntryRPC &resp_ae) {
        int term = resp_ae.term();
        if (term_ < term) {
            if (state_ == candidate || state_ == primary) {
                for (auto &timers : retry_timers_) {
                    timers.second.cancel();
                }
            }
            trans2follower(term);
        } else if (term_ > term) {
            //do nothing, even with the correct rpc_lsn, the primary can't do anything for safety, not even response
        } else {
            if (state_ == primary) {

            }
        }
    }

    void react2_AE(tuple<string, int> server, AppendEntryRpc append_rpc) {
        int term = append_rpc.term();

        raft_rpc::rpc_Entry entry = append_rpc.entry(); //这里有点蛋疼，protobuf很难用，先放着，假设抽象好了
        int prelog_term = append_rpc.prelog_term();
        int prelog_index = append_rpc.prelog_index();
        int commitIndex = append_rpc.commit_index();
        // 这行错了!       assert(commitIndex <= prelog_index); 完全有可能出现commitIndex比prelog_index大的情况
        if (term_ > term) {
            resp2entry(false, "react2_AE, term_ > term", server);
            return;
        } else { //term_ <= term
            trans2follower(term);//会设置定时器，所以此函数不需要设置定时器了
            //一开始想法是commitIndex直接赋值给本地commitIndex，然后本地有一个线程直接根据本第commitIndex闷头apply to stateMachine就行了（因为两者完全正交），
            // 但是一想不对，因为rpc的commitIndex可能比prelog_index大，假设本地和主同步的entry是0到20，21到100都是stale的entry，rpc传来一个commitIndex为50，prelog_index为80（还没走到真确的20），难道那
            // 个线程就无脑把到80的entry给apply了？所以本地的commitIndex只能是
            unsigned last_index = entries_.last_index();
            if (last_index < prelog_index) {
                resp2entry(false, "react2_AE, term is ok, but follower's last entry index is " + to_string(last_index) + ", but prelog_index is " + to_string(prelog_index), server);
                return;
            } else { // if (last_index >= prelog_index)  注意> 和 = 的情况是一样的，（至少目前我是这么认为的）
                if (entries_[prelog_index].Term() <= prelog_term) {
                    entries_.update_commit_index(commitIndex, prelog_index); //commitIndex永远小于prelog_index，而prelog_index能够复制又表示之前的index都已经ready了，所以可以修改commitIndex了
                    entries_.insert(prelog_index, entry);
                    resp2entry(true, "成功insert", server);
                } else {
                    throw "出现了完全不可能出现的情况"; //其实是有可能的，比如在网络滞留了很久的rpc的
                }
            }
        }
    }


//    follower 特有的
    //可以设置为bool，因为raft规定一个term只能一个主, 考虑这种情况，candidate发出了rv，但是所有f只能接受rv，不能发会resp_rv,但是我的设定中f在收到相同term的rv时不会重传，因为c会自动增加term再rv，只有收到了更高term的rv，f才会resp，所以这里只用bool就够了
    bool F_already_voted;

//   candidate 特有的
    int winned_votes_;

    //primary 特有的
    map<tuple<string, int>, int> nextIndex_;
    map<tuple<string, int>, int> Follower_saved_index;


    //follower 和 candidate 才需要的
    RPC network_;
    deadline_timer timer_candidate_expire_;
    int term_;
    State state_;
    StateMachine state_machine_;
    io_context &ioContext;
    Entries entries_;

    map<std::tuple<string, int>, int> current_rpc_lsn_; //if resp_rpc's lsn != current[server]; then just ignore; (send back the index can't not ensuring idempotence; for example we received a resp which wandereding in the network for 100 years)
    map<std::tuple<string, int>, deadline_timer> retry_timers_;
    vector<std::tuple<string, int>> configuration_;
    string ip_;
    int port_;

private:


    void trans2P() {
        //todo cancel all timers(maybe, need a deeper think)
        state_ = primary;
        for (auto server: configuration_) {
            AE(server);
        }
    }

    string build_rpc_ae_string(tuple<string, int> server) {
        unsigned int server_current_rpc_lsn = current_rpc_lsn_[server];
        AppendEntryRpc rpc;
        rpc.set_lsn(server_current_rpc_lsn);
        rpc.set_term(term_);
        int index_to_send = nextIndex_[server];
        rpc.set_commit_index(entries_.get_commit_index());
        if (index_to_send > 0) {
            rpc_Entry prev_entry = entries_.get(index_to_send - 1);
            rpc.set_prelog_term(prev_entry.get_term());
        } else if (index_to_send <= 0) {
            rpc.set_prelog_term(-1);
        }

        if (index_to_send == entries_.size()) {
            //empty rpc as HB
        } else {
            rpc_Entry entry = entries_.get(index_to_send);  //如何做Entry到protobuf的映射？
            rpc.set_rpc_Entry(entry);  //有空看下protobuf是怎么用 这个nested的变量的
        }
        string msg;
        rpc.SerializeToString(&msg);
        int len = 4 + msg.size();
        return to_string(len) + to_string(1) + msg;
    }

    void AE(tuple<string, int> server) { //todo 设定重传timer！
        string ae_rpc_str = build_rpc_ae_string(server);
        writeTo(server, ae_rpc_str);   //考虑如下场景，发了AE1,index为100,然后定时器到期重发了AE2,index为100，这时候收到ae1的resp为false，将index--,如果ae2到F，又触发了一次resp，这两个rpc的请求一样，resp一样，但是P收到两个一样的resp会将index--两次
        deadline_timer &t1 = retry_timers_[server];
        int waiting_counts = t1.expires_from_now(boost::posix_time::seconds(random_ae_retry_expire()));
        if (waiting_counts != 0) {
            throw "should be zero, a timer can't be interrupt by network, but when AE is called, there should be no hooks on the timer";
        } else {
            t1.async_wait([this, server](const boost::system::error_code &error) {
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
    }

    // this is term_ == remote term, otherwise we deal somewhere else(actually just deny or trans2F)
    void primary_deal_resp_ae(tuple<string, int> server, Resp_AppendEntryRPC &resp_ae) {
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
                if (last_send_index >= entries_.size()) {
                    // notice > and ==, we don't send next index immediately
                    //do nothing, let the timers expires and send empty rps_ae as heartbeat, that situation 2 in AE()
                } else {
                    //cancel timer todo check hook num
                    retry_timers_[server].cancel();
                    // immediately the next AE
                    AE(server);
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
                    retry_timers_[server].cancel();
                    AE(server);
                }

            }
        }
    }


    void request_votes() {
        for (auto &server : configuration_) {
            RV(server);
        }
    }

    string build_rpc_rv_string(tuple<string, int> server) {
        unsigned int server_current_rpc_lsn = current_rpc_lsn_[server];
        RequestVoteRpc rpc;
        rpc.set_lsn(server_current_rpc_lsn);
        rpc.set_term(term_);
        rpc.set_index(entries_.size() - 1);
        string msg;
        rpc.SerializeToString(&msg);
        int len = 4 + msg.size();
        //这里有个bug我们先不处理，在msg特别长的时候会出现
        return to_string(len) + to_string(1) + msg;
    }

    void RV(tuple<string, int> server) {
        string rv_rpc_str = build_rpc_rv_string(server);
        writeTo(server, rv_rpc_str);
        deadline_timer &timer = retry_timers_[server];
        int waiting_counts = timer.expires_from_now(boost::posix_time::seconds(random_rv_retry_expire()));
        if (waiting_counts != 0) {
            throw "should be zero, a timer can't be interrupt by network, but when AE is called, there should be no hooks on the timer";
        } else {
            timer.async_wait([this, server](const boost::system::error_code &error) {
                if (error == boost::asio::error::operation_aborted) {
                    //do nothing, maybe log
                } else {
                    RV(server);
                }
            });
        }
    }

    void candidate_deal_resp_rv(tuple<string, int> server, Resp_RequestVoteRpc &resp_rv) {
        int resp_lsn = resp_rv.lsn();
        int current_rpc_lsn = current_rpc_lsn_[server];
        if (current_rpc_lsn != resp_lsn) {
            return;
        } else {
            bool ok = resp_rv.ok();
            if (ok) {
                winned_votes_++;
                if (winned_vote2>){

                }else{
                    // cancel the timer
                }

            } else {

            }
        }
    }


private:
    //mock 一下先
    void load_config_from_file() {
        configuration_ = {
                {"locahost", 8888},
                {"locahost", 7777},
                {"locahost", 9999},
        };

    }
};

int main() {
    init_logging();
    BOOST_LOG_TRIVIAL(trace) << "This is a trace severity message";
    BOOST_LOG_TRIVIAL(debug) << "This is a debug severity message";
    BOOST_LOG_TRIVIAL(info) << "This is an informational severity message";
    BOOST_LOG_TRIVIAL(warning) << "This is a warning severity message";
    BOOST_LOG_TRIVIAL(error) << "This is an error severity message";
    BOOST_LOG_TRIVIAL(fatal) << "and this is a fatal severity message";
    return 0;
}