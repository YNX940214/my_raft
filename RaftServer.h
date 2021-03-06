//
// Created by ynx on 2020-02-09.
//

#ifndef RAFT_RAFTSERVER_H
#define RAFT_RAFTSERVER_H

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
#include "Timer.h"
#include <vector>
#include <fstream>
#include "state_machine/StateMachineControler.h"
#include "state_machine/StateMachine.h"

using namespace boost::asio;
using namespace std;
using boost::asio::ip::tcp;
using namespace raft_rpc;
enum State {
    follower, candidate, primary
};

class RaftServer {
public:

    RaftServer(io_service &loop, const string &_ip, int _port, string config_path, StateMachine *sm);

    void run();

    void write_to_client(int entry_index, const string res_str);

    void write_to_client(std::shared_ptr<tcp::socket> socket, const string res_str);

private:
    /*
     * return True the rpc lsn is smaller than local stored lsn, we should ignore it.
     */
    bool should_ignore_remote_rpc(const tuple<string, int> &remote, int new_lsn);

    void reset_both_rpc_lsn();

    void update_term(int term);

    void load_config_from_file();

    void cancel_all_timers();

    void trans2F(int term);

    void trans2P();

    void trans2C();

    string build_rpc_ae_string(const tuple<string, int> &server);

    void AE(const tuple<string, int> &server);

    string build_rpc_rv_string(const tuple<string, int> &server);

    void RV(const tuple<string, int> &server);

    void reactToIncomingMsg(RPC_TYPE _rpc_type, const string msg, const tuple<string, int> &peer_addr);

    void get_from_state_machine(const tuple<string, int> &client_addr, string client_query_str);

    void append_client_apply_to_entries(const tuple<string, int> &client_addr, string apply_str);

    void react2ae(AppendEntryRpc rpc_ae);

    void react2rv(RequestVoteRpc rpc_rv);

    void react2resp_ae(Resp_AppendEntryRpc &resp_ae);

    void react2resp_rv(Resp_RequestVoteRpc &resp_rv);

    void make_resp_rv(bool vote, string context_log, tuple<string, int> server, int lsn);

    void make_resp_ae(bool ok, string context_log, tuple<string, int> server, int lsn, bool is_empty_ae);

    void primary_deal_resp_ae_with_same_term(const tuple<string, int> &server, Resp_AppendEntryRpc &resp_ae);

    void candidate_deal_resp_rv_with_same_term(const tuple<string, int> &server, Resp_RequestVoteRpc &resp_rv);

    void apply_to_state_machine(int new_commit_index);

    inline void primary_update_commit_index(const tuple<string, int> &server, int index);

    //我们必须相信主传来的remote_commit_index,因为当从accept 一个ae的时候，这个ae是验证过preLog的term的，也就是说prelog已经全部想等了，
    //首先，如果主不判断follower_save_index就直接把集群已经commit的index传来，那么commit_index是可能>= F's entry size, 但是从可以通过与自己的entry size比较来更新commit index，
    // 不过我选择在主用follower_saved_index做一次判断再发来commit_index，做一个鱼类，让从暴露出我们的逻辑问题
    inline void follower_update_commit_index(int remote_commit_index);


    void write_resp_apply_call(int entry_index, const string res_str);

    void write_resp_query_call(const std::tuple<string, int> &addr, const string res_str);

    //    void deal_with_write_error(boost::system::error_code &ec, std::size_t);

private:
    int winned_votes_;
    //可以设置为bool，因为raft规定一个term只能一个主, 考虑这种情况，candidate发出了rv，但是所有f只能接受rv，不能发会resp_rv,但是我的设定中f在收到相同term的rv时不会重传，因为c会自动增加term再rv，只有收到了更高term的rv，f才会resp，所以这里只用bool就够了
    bool already_voted_;
    std::tuple<string, int> known_primary_; //todo
    string config_path_;


    //normal
    int term_;
    State state_;
    io_context &ioContext;
    vector<std::tuple<string, int>> configuration_;
    int majority_;
    int N_;


    //rpc
    string ip_;
    int port_;
    tcp::endpoint listen_point_;
    std::tuple<string, int> server_;
    RPC network_;

    /*
     * both there rpcs should be reset when term grows, (but AE or HB will trans F to F with same
     * term, but that shouldn't reset the followers rpc_lsn for example), this lsn is just used to ensure
     * stale rpc or resp_rpc within the same term will not be reacted, the term always has a high logic process priority
     * than rpc lsn, because the logic deal with term will block the stale rpc and resps with lower term,
     *
     *
     *
     * So !!! Remember！！！！ ALWAYS put the logic check of lsn BEHIND term checks!!!!! 令我惊讶的是在我写完前一句话的时候去检查了一下以前的逻辑，竟然是符合lsn在term之后的
     *
     * AND!! reset all both lsn when term grows!
     *
     */

    // this rpc_lsn can only be modified by P, this is used to deal with stale resp_ae or stale resp_rv, P will not react to stale resps,
    map<std::tuple<string, int>, int> current_rpc_lsn_; //if resp_rpc's lsn != current[server]; then just ignore; (send back the index can't not ensuring idempotence; for example we received a resp which wandereding in the network for 100 years)
    // this rpc_lsn can only be modified by F, this is used to deal with Scene 111, F will not react to stale rpc calls.
    map<std::tuple<string, int>, int> follower_rpc_lsn_;


    //timers
    deadline_timer candidate_timer_;
    map<std::tuple<string, int>, shared_ptr<Timer>> retry_timers_;

    // log & state machine
    int commit_index_;
    Entries entries_;
    map<tuple<string, int>, int> nextIndex_;
    map<tuple<string, int>, int> Follower_saved_index; // work for candidate

    //clients
    StateMachineControler smc_;  //定义在entries_和state_machine后面

    /*
     * 让rpc层专心做 addr到socket的映射，
     * raft_server做entry index到 addr的映射
     */
    map<int, std::tuple<string, int>> entryIndex_to_socketAddr_map_;

    friend class StateMachineControler;
};


#endif //RAFT_RAFTSERVER_H
