//
// Created by ynx on 2019-11-19.
//

#ifndef RAFT_UTIL_H
#define RAFT_UTIL_H

#define Log_trace       BOOST_LOG_TRIVIAL(trace) << __FUNCTION__ << " "
#define Log_debug       BOOST_LOG_TRIVIAL(debug) << __FUNCTION__ << " "
#define Log_info         BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << " "
#define Log_warning        BOOST_LOG_TRIVIAL(warning) << __FUNCTION__ << " "
#define Log_error       BOOST_LOG_TRIVIAL(error) << __FUNCTION__ << " "
#define Log_fatal         BOOST_LOG_TRIVIAL(fatal) << __FUNCTION__ << " "


using std::string;

int random_candidate_expire();

int random_rv_retry_expire();

int random_ae_retry_expire();

unsigned smaller(unsigned a, unsigned b);

bool file_exists(const string &path);

string server2str(const std::tuple<string,int> & server);

//string rpc2str(string msg);
#endif //RAFT_UTIL_H
