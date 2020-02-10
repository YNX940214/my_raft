//
// Created by ynx on 2020-02-01.
//

#ifndef RAFT_STATEMACHINE_H
#define RAFT_STATEMACHINE_H

#include <string>

using std::string;

class StateMachine {
public:

    virtual string apply(const string &encoded_apply_str) = 0;

    virtual string get(const string &encoded_get_str) = 0;

    virtual string build_apply_str(const string &key, int v) = 0;

    virtual string build_query_str(const string &key) = 0;

    virtual void react_to_resp_query(const string &resp_query) = 0;

    virtual void react_to_resp_apply(const string &resp_apply) = 0;
};


#endif //RAFT_STATEMACHINE_H
