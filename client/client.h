//
// Created by ynx on 2020-02-05.
//

#ifndef RAFT_CLIENT_H
#define RAFT_CLIENT_H

#include "../state_machine/StateMachine.h"
#include "../rpc/rpc.h"

class client {
public:
    client(io_service &ioContext, const std::tuple<string, int> &server, StateMachine *sm);

    void run();

    void set_server(std::tuple<string, int> server);

    void apply_to_server();

    void query_from_server();

    void react_to_resp(RPC_TYPE rpc_type, const string &resp_str, const tuple<string, int> &addr);

    void react_to_resp_query(const string &resp_query_str);

    void react_to_resp_apply(const string &resp_apply_str);

//    void prompt_for_apply();

private:
    std::tuple<string, int> server_;
    io_service &ioContext_;
    StateMachine *sm_;
    RPC network_;
};


#endif //RAFT_CLIENT_H
