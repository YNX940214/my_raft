//
// Created by ynx on 2019-11-19.
//

#ifndef RAFT_UTIL_H
#define RAFT_UTIL_H


using std::string;

int random_candidate_expire();

int random_rv_retry_expire();

int random_ae_retry_expire();

unsigned smaller(unsigned a, unsigned b);

bool file_exists(const string &path);

string server2str(const std::tuple<string,int> & server);
#endif //RAFT_UTIL_H
