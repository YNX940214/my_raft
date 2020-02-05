//
// Created by ynx on 2020-02-05.
//

#ifndef RAFT_CLIENT_H
#define RAFT_CLIENT_H

#include "../state_machine/EasyStateMachine.h"
#include "../rpc/rpc.h"

class client {
public:
    bool apply_to_server();

    string query_from_server();

    void react_to_resp_query(string resp_query_str);

    void react_to_resp_apply(string resp_apply_str);

private:
    std::tuple<string, int> server_;

};


#endif //RAFT_CLIENT_H
