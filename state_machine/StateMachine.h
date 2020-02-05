//
// Created by ynx on 2020-02-01.
//

#ifndef RAFT_STATEMACHINE_H
#define RAFT_STATEMACHINE_H

#include <string>

using std::string;

class StateMachine {
public:

    virtual string apply(string encoded_apply_str) = 0;

    virtual string get(string encoded_get_str) = 0;

};


#endif //RAFT_STATEMACHINE_H
