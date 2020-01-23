//
// Created by ynx on 2019-11-19.
//

#ifndef RAFT_UTIL_H
#define RAFT_UTIL_H

#include <iostream>
#include <string>

using std::string;

inline int random_candidate_expire() {
    int max = 300;
    int min = 200;
    int range = max - min + 1;
    int num = rand() % range + min;

    return num;
}

inline int random_rv_retry_expire() {
    int max = 300;
    int min = 200;
    int range = max - min + 1;
    int num = rand() % range + min;

    return num;
}

inline int random_ae_retry_expire() {
    int max = 300;
    int min = 200;
    int range = max - min + 1;
    int num = rand() % range + min;
    return num;
}

inline unsigned smaller(unsigned a, unsigned b) {
    if (a < b) {
        return a;
    } else {
        return b;
    }
}

inline bool file_exists(const string &path) {
    FILE *fp = fopen(path.c_str(), "r");
    if (fp) {
        return true;
    } else {
        return false;
    }
}

#endif //RAFT_UTIL_H
