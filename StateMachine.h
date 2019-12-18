//
// Created by ynx on 2019-12-18.
//

#ifndef RAFT_STATEMACHINE_H
#define RAFT_STATEMACHINE_H

#include "entry.h"

class StateMachine {
public:

private:
    const Entries &entries;
    int state;
};


#endif //RAFT_STATEMACHINE_H
