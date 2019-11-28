#include <iostream>
#include <vector>
#include <string>
#include <boost/asio.hpp>
#include "util.h"

using namespace boost::asio;
using namespace std;
using boost::asio::ip::tcp;

enum State {
    follower, candidate, primary
};


class Entry {
public:
    string msg;
    int term;
    int index;
};
//-1。 目前最当务之急的问题是弄清楚如何rpc，我现在是sendback这样操作的（这是最自然的，这个sendback是对触发这个函数的rpc的sendback，如何抽象是个问题）
//并考虑如下例子中的场景，a 发出vote rpc， b收到，发回， 回到前不久a收到qurom的vote，变身成主，如何用rpc表示。思考sendback除了ok和term是否还要返回别的信息

//0. log 是个问题，boost的log编译失败 log的基本修养 time level file line function msg
//2. entry是个class，操作entry会写入磁盘，是一个对文件的封装
//3. 先把程序跑起来，做出第一次选举。
//4。根据commitIndex把entry apply到state machine的独立线程，注意这个线程只能读commitIndex不能改
class instance {
public:
    instance(io_service &loop) : ioContext(loop), A_term(0), A_state(follower), F_already_voted(false), A_timer_candidate_expire(loop), C_grantedVoteNum(0), A_commitIndex(0),
                                 A_state_machine(), A_acceptor(loop) {}

    void run() {
        int waiting_counts = A_timer_candidate_expire.expires_from_now(
                boost::posix_time::seconds(random_candidate_expire()));
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
            throw ("trans2candidate只能是timer_candidate_expire自然到期触发，所以不可能有waiting_count不为0的情况，否则就是未考虑的情况")
        }
        requestVotesCall();
    }

    void follower_react2_VoteRPC(int term, int logindex, int logterm) {
        if (A_term > term) {
            //how to do RPC ??????????
//            send back (false, term)
            return;
        }
        if (A_term < term) {
            //trans to follower,
            trans2follower(term);
//          no return; 不return，因为还要投票
        }
        //这里是A_term <= term的情况，
        if (F_already_voted == false) {  //这个if针对 A_term == term 的情况，只有一票
            bool should_vote = false;
            if (entries.size() == 0) { //只管投票
                should_vote = true;
            } else {
                Entry lastEntry = entries[entries.size() - 1];
                if (lastEntry.term > logterm) {
                    should_vote = false;
                } else if (lastEntry.term < logterm) {
                    should_vote = true;
                } else {
                    if (lastEntry.index > logindex) {
                        should_vote = false;
                    } else {
                        should_vote = true;
                    }
                }
            }
            if (should_vote) {
                //sendback(true)
            } else {
                //sendback(false)
            }
        } else {
//            sendback(false, term) //这里作弊了，直接看了js，发现不会触发任何定时器的更改
        }


    }

    void follower_react_2_AppendEntry_RPC(int term, Entry entry, int prelog_term, int prelog_index, int commitIndex) {
        // 这行错了!       assert(commitIndex <= prelog_index); 完全有可能出现commitIndex比prelog_index大的情况
        if (A_term > term) {
//            sendback(false, A_term))
            return; //啥都不做，退出
        } else { //A_term <= term
            trans2follower(term);//会设置定时器，所以此函数不需要设置定时器了
            //一开始想法是commitIndex直接赋值给本地commitIndex，然后本地有一个线程直接根据本第commitIndex闷头apply to stateMachine就行了（因为两者完全正交），
            // 但是一想不对，因为rpc的commitIndex可能比prelog_index大，假设本地和主同步的entry是0到20，21到100都是stale的entry，rpc传来一个commitIndex为50，prelog_index为80（还没走到真确的20），难道那
            // 个线程就无脑把到80的entry给apply了？所以本地的commitIndex只能是
            int last_index = entries.size() - 1;
            if (last_index < prelog_index) {
                // sendback(false,A_term)
                return;
            } else { // if (last_index >= prelog_index)  注意> 和 = 的情况是一样的，（至少目前我是这么认为的）
                if (entries[prelog_index].term < prelog_term) {
                    //no entry.delete_behind 并不需要操作entry，等到相等的term再操作也不迟
                    //sendback(false, A_term) //这里的term还有用么。。。 已经变成Primary的term了
                    //no commitIndex=commitIndex;
                    return;
                } else if (entries[prelog_index].term == prelog_term) {
                    // no commitIndex = commitIndex; //还不能跟新，如果apply 线程神速怎么办，还没来的急会滚就apply了
                    //sendback(true,term);
                    //entry.delete_behind(prelog_index)
                    //entry.append(entry)
                    A_commitIndex = commitIndex;  //stale的数据回滚了，现在apply线程可以开始干活了
                } else {
                    throw "出现了完全不可能出现的情况";
                }
            }
        }
    }

    void cb_accept(const boost::){

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
    deadline_timer A_timer_candidate_expire;
    int A_term;
    State A_state;
    vector<Entry> A_enties; //todo 硬盘or内存？
    int A_commitIndex;//都需要报错，从follower上位成primary需要
    StateMachine A_state_machine;
    tcp::acceptor A_acceptor;
    io_context &ioContext;
    Entries entries;
    // todo configuration list:{ip,port}

};

int main() {
    std::cout << "Hello, World!" << std::endl;
    return 0;
}