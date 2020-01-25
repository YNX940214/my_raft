//
// Created by ynx on 2019-11-19.
//

#ifndef RAFT_UTIL_H
#define RAFT_UTIL_H

#include "util.h"
#include <iostream>
#include <string>

using std::string;

int random_candidate_expire() {
    int max = 300;
    int min = 150;
    int range = max - min + 1;
    int num = rand() % range + min;

    return num;
}

int random_rv_retry_expire() {
    int max = 10;
    int min = 5;
    int range = max - min + 1;
    int num = rand() % range + min;

    return num;
}

int random_ae_retry_expire() {
    int max = 10;
    int min = 5;
    int range = max - min + 1;
    int num = rand() % range + min;
    return num;
}

unsigned smaller(unsigned a, unsigned b) {
    if (a < b) {
        return a;
    } else {
        return b;
    }
}

bool file_exists(const string &path) {
    FILE *fp = fopen(path.c_str(), "r");
    if (fp) {
        return true;
    } else {
        return false;
    }
}

string server2str(const std::tuple<string, int> &server) {
    string ip = std::get<0>(server);
    int port = std::get<1>(server);
    return ip + ":" + std::to_string(port);
}

#endif //RAFT_UTIL_H
