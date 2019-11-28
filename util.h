//
// Created by ynx on 2019-11-19.
//

#ifndef RAFT_UTIL_H
#define RAFT_UTIL_H

#include <iostream>

int random_candidate_expire(){
    int max=300;
    int min=200;
    int range = max - min + 1;
    int num = rand() % range + min;

    return num;
}
#endif //RAFT_UTIL_H
