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

    RaftServer(io_service &loop, const string &_ip, int _port, const tcp::endpoint &_endpoint, string config_path, StateMachine *sm);

    void run();

    void write_to_client(unsigned int entry_index, const string res_str);

    void write_to_client(std::shared_ptr<tcp::socket> socket, const string res_str);

private:
    void load_config_from_file();

    void cancel_all_timers();

    void trans2F(int term);

    void trans2P();

    void trans2C();

    string build_rpc_ae_string(const tuple<string, int> &server);

    void AE(const tuple<string, int> &server);

    string build_rpc_rv_string(const tuple<string, int> &server);

    void RV(const tuple<string, int> &server);

    void reactToIncomingMsg(RPC_TYPE _rpc_type, const string msg, std::shared_ptr<tcp::socket> client_socket_sp);

    void get_from_state_machine(std::shared_ptr<tcp::socket> client_peer, string client_query_str);

    void append_client_apply_to_entries(std::shared_ptr<tcp::socket> client_peer, string apply_str);

    void react2ae(AppendEntryRpc rpc_ae);

    void react2rv(RequestVoteRpc rpc_rv);

    void react2resp_ae(Resp_AppendEntryRpc &resp_ae);

    void react2resp_rv(Resp_RequestVoteRpc &resp_rv);

    void make_resp_rv(bool vote, string context_log, tuple<string, int> server, int lsn);

    void make_resp_ae(bool ok, string context_log, tuple<string, int> server, int lsn);

    void primary_deal_resp_ae_with_same_term(const tuple<string, int> &server, Resp_AppendEntryRpc &resp_ae);

    void candidate_deal_resp_rv_with_same_term(const tuple<string, int> &server, Resp_RequestVoteRpc &resp_rv);

    void apply_to_state_machine(unsigned int new_commit_index);

    inline void primary_update_commit_index(const tuple<string, int> &server, int index);

    inline void follower_update_commit_index(unsigned remote_commit_index, unsigned remote_prelog_index);

    void writeTo(RPC_TYPE rpc_type, const tuple<string, int> &server, const string &msg);

    void deal_with_write_error(boost::system::error_code &ec, std::size_t);

    void write_resp_apply_call(unsigned int entry_index, const string res_str);

    void write_resp_query_call(std::shared_ptr<tcp::socket> socket, const string res_str);

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
    tcp::endpoint endpoint_;
    string ip_;
    int port_;
    std::tuple<string, int> server_;
    map<std::tuple<string, int>, int> current_rpc_lsn_; //if resp_rpc's lsn != current[server]; then just ignore; (send back the index can't not ensuring idempotence; for example we received a resp which wandereding in the network for 100 years)
    RPC network_;

    //timers
    deadline_timer candidate_timer_;
    map<std::tuple<string, int>, shared_ptr<Timer>> retry_timers_;

    // log & state machine
    int commit_index_;
    Entries entries_;
    map<tuple<string, int>, int> nextIndex_;
    map<tuple<string, int>, int> Follower_saved_index;

    //clients
    StateMachineControler smc_;  //定义在entries_和state_machine后面

    /*
     * 让rpc层专心做 addr到socket的映射，
     * raft_server做entry index到 addr的映射
     */
    map<unsigned int, std::tuple<string, int>> entryIndex_to_socketAddr_map_;

    friend class StateMachineControler;
};


#endif //RAFT_RAFTSERVER_H
