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
    instance(io_service &loop) : ioContext(loop),
                                 A_term(0),
                                 A_state(follower),
                                 F_already_voted(false),
                                 A_timer_candidate_expire(loop),
                                 C_grantedVoteNum(0),
                                 A_state_machine(),
                                 A_network(loop, std::bind(&instance::reactToIncomingMsg, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4)) {}

    void run() {
        A_network.startAccept();
        int waiting_counts = A_timer_candidate_expire.expires_from_now(boost::posix_time::seconds(random_candidate_expire()));
        if (waiting_counts == 0) {
            throw "初始化阶段这里不可能为0";
        } else {
            A_timer_candidate_expire.async_wait([this](const boost::system::error_code &error) {
                if (error == boost::asio::error::operation_aborted) {
                    //可能会被在follow_deal_VoteRPC中取消，或者ARPC中取消，但是按照目前看来被cancel什么都不用管，把执行流让给主动cancel定时器的函数就行了
                } else {
                    trans2candidate();
                }
            });
        }
        ioContext.run();
    }

    void writeTo(string ip, int port, string msg) {
        A_network.writeTo(ip, port, msg, std::bind);
    }

    void reactToIncomingMsg(RPC_TYPE rpc_type_, string msg, string ip, int port) {
        //proto buf decode
        if (rpc_type_ == REQUEST_VOTE) {
            RequestVoteRpc voteRpc;
            voteRpc.ParseFromString(msg);
            follower_react2_VoteRPC(ip, port, voteRpc);
        } else if (rpc_type_ == APPEND_ENTRY) {
            AppendEntryRpc append_rpc;
            append_rpc.ParseFromString(msg);
            follower_react_2_AppendEntry_RPC(ip, port, append_rpc);
        } else if (rpc_type_ == RESP_VOTE) {

        } else if (rpc_type_ == RESP_APPEND) {

        } else {
            BOOST_LOG_TRIVIAL(error) << "unknown action: " << rpc_type_ << ", the whole msg string is:\n" + msg;
        }
    }

    void trans2follower(int term) {
        A_term = term;
        A_state = follower;
        int waiting_count = A_timer_candidate_expire.expires_from_now(boost::posix_time::seconds(random_candidate_expire()));
        if (waiting_count == 0) {
            throw "trans2follower是因为收到了外界rpc切换成follower，所以至少有一个cb_tran2_candidate的定时器，否则就是未考虑的情况";
        }
        A_timer_candidate_expire.async_wait([this](const boost::system::error_code &error) {
            if (error == boost::asio::error::operation_aborted) {
                //啥都不做，让react处理
            } else {
                trans2candidate();
            }
        });
    }

    void trans2candidate() {
        A_term++;
        A_state = candidate;
        C_grantedVoteNum = 1;
        //设置下一个term candidate的定时器
        int waiting_count = A_timer_candidate_expire.expires_from_now(boost::posix_time::seconds(random_candidate_expire()));
        if (waiting_count == 0) {
            //完全可能出现这种情况，如果是cb_trans2_candidate的定时器自然到期，waiting_count就是0
            //leave behind
        } else {
            throw ("trans2candidate只能是timer_candidate_expire自然到期触发，所以不可能有waiting_count不为0的情况，否则就是未考虑的情况");
        }
        requestVotesCall();
    }

    void resp_2_vote(bool vote, string context_log, string ip, int port) {
        Resp_RequestVoteRpc resp_vote;
        resp_vote.set_ok(vote);
        resp_vote.set_term(A_term);
        string msg;
        resp_vote.SerializeToString(&msg);
        A_network.writeTo(ip, port, msg, [context_log](boost::system::error_code &ec, std::size_t) {
            if (!ec) {
                BOOST_LOG_TRIVIAL(debug) << "callback of '" << context_log << "' done";
            } else {
                BOOST_LOG_TRIVIAL(info) << "callback of '" << context_log << "' got an  error: " << ec;
            }
        });
    }


    //term 没用
    void resp2entry(bool ok, string context_log, string ip, int port) {
        Resp_AppendEntryRPC resp_entry;
        resp_entry.set_ok(ok);
        resp_entry.set_term(A_term);
        string msg;
        resp_entry.SerializeToString(&msg);
        A_network.writeTo(ip, port, msg, [context_log](boost::system::error_code &ec, std::size_t) {
            if (!ec) {
                BOOST_LOG_TRIVIAL(debug) << "callback of '" << context_log << "' done";
            } else {
                BOOST_LOG_TRIVIAL(info) << "callback of '" << context_log << "' got an  error: " << ec;
            }
        });
    }


    void follower_react2_VoteRPC(string ip, int port, RequestVoteRpc vote_rpc) {
        int term = vote_rpc.term();
        int vote_index = vote_rpc.index();
        int vote_term = vote_rpc.term();
        if (A_term > term) {
            resp_2_vote(false, "follower_react2_VoteRPC: A_term > term,  write back false", ip, port);
            return;
        }
        if (A_term < term) {
            trans2follower(term);
        }
        if (!F_already_voted) {  //这个if针对 A_term == term 的情况，只有一票
            bool should_vote = false;
            if (A_entries.size() == 0) { //只管投票
                should_vote = true;
            } else {
                Entry &lastEntry = A_entries.last_entry();
                if (lastEntry.Term() > vote_term) {
                    should_vote = false;
                } else if (lastEntry.Term() < vote_term) {
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
                resp_2_vote(false, "follower_react2_VoteRPC: A vote is send", ip, port);
            } else {
                resp_2_vote(false, "follower_react2_VoteRPC: A_vote is denied", ip, port);
            }
        } else {
            resp_2_vote(false, "follower_react2_VoteRPC: No votes left this term", ip, port);
        }
    }

    void follower_react_2_AppendEntry_RPC(string ip, int port, AppendEntryRpc append_rpc) {
        int term = append_rpc.term();
        raft_rpc::Entry entry = append_rpc.entry();
        int prelog_term = append_rpc.prelog_term();
        int prelog_index = append_rpc.prelog_index();
        int commitIndex = append_rpc.commit_index();
        // 这行错了!       assert(commitIndex <= prelog_index); 完全有可能出现commitIndex比prelog_index大的情况
        if (A_term > term) {
            resp2entry(false, "follower_react_2_AppendEntry_RPC, A_term > term", ip, port);
            return;
        } else { //A_term <= term
            trans2follower(term);//会设置定时器，所以此函数不需要设置定时器了
            //一开始想法是commitIndex直接赋值给本地commitIndex，然后本地有一个线程直接根据本第commitIndex闷头apply to stateMachine就行了（因为两者完全正交），
            // 但是一想不对，因为rpc的commitIndex可能比prelog_index大，假设本地和主同步的entry是0到20，21到100都是stale的entry，rpc传来一个commitIndex为50，prelog_index为80（还没走到真确的20），难道那
            // 个线程就无脑把到80的entry给apply了？所以本地的commitIndex只能是
            unsigned last_index = A_entries.last_index();
            if (last_index < prelog_index) {
                resp2entry(false, "follower_react_2_AppendEntry_RPC, term is ok, but follower's last entry index is " + to_string(last_index) + ", but prelog_index is " + to_string(prelog_index), ip, port);
                return;
            } else { // if (last_index >= prelog_index)  注意> 和 = 的情况是一样的，（至少目前我是这么认为的）
                if (A_entries[prelog_index].Term() <= prelog_term) {
                    A_entries.update_commit_index(commitIndex, prelog_index);
                    A_entries.insert(prelog_index, entry);
                    resp2entry(true, "成功insert", ip, port);
                } else {
                    throw "出现了完全不可能出现的情况";
                }
            }
        }
    }


//    需要id作为入参数么，Follower只根据term判断是否可能接受log，如果保证一个term只有一个Primary，则不需要id，（由于我的想法中是这样的，所以先不加上）
    void AppendEntry_RPC(int term, int prelog_term, int prevlog_index, string log) {
//      send
//      {
//          "term":term,
//          "prelog_term":prelog_term,
//          "prevlog_index":prelog_index,
//          "log":log
//      }

//      rep
//        {
//            "term":int
//            "ok":bool,
//        }
//        return rep.ok
    }

//    follower 特有的
    //可以设置为bool，因为raft规定一个term只能一个主
    bool F_already_voted;

//   candidate 特有的
    int C_grantedVoteNum;

    //primary 特有的
    vector<int> P_nextIndex;
    vector<int> P_follow_saved_index;


    //follower 和 candidate 才需要的
    RPC A_network;
    deadline_timer A_timer_candidate_expire;
    int A_term;
    State A_state;
    StateMachine A_state_machine;
    io_context &ioContext;
    Entries A_entries;
    // todo configuration list:{ip,port}

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