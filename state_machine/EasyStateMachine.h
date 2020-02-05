//
// Created by ynx on 2020-02-05.
//

#ifndef RAFT_EASYSTATEMACHINE_H
#define RAFT_EASYSTATEMACHINE_H

#include "StateMachine.h"
#include "easy_state_machine.pb.h"
#include <map>
#include <string>


using std::string;

class EasyStateMachine : StateMachine {
public:
    static string build_apply_str(const string &key, int v);

    static string build_query_str(const string &key);

    string apply(const string &encoded_apply_str);

    string get(const string &encoded_get_str);

private:
    std::map<string, int> mock_state_machine_;

};


#endif //RAFT_EASYSTATEMACHINE_H
