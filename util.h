//
// Created by ynx on 2019-11-19.
//

#ifndef RAFT_UTIL_H
#define RAFT_UTIL_H

#include "rpc.pb.h"
#include <boost/asio.hpp>
#include "./log/boost_lop.h"

#define Log_trace       BOOST_LOG_TRIVIAL(trace) << __FILE__ << " [" << __FUNCTION__ << "] "
#define Log_debug       BOOST_LOG_TRIVIAL(debug) << __FILE__ << " [" << __FUNCTION__ << "] "
#define Log_info         BOOST_LOG_TRIVIAL(info) << __FILE__ << " [" << __FUNCTION__ << "] "
#define Log_warning        BOOST_LOG_TRIVIAL(warning) << __FILE__ << " [" << __FUNCTION__ << "] "
#define Log_error       BOOST_LOG_TRIVIAL(error) <<__FILE__ << " [" << __FUNCTION__ << "] "
#define Log_fatal         BOOST_LOG_TRIVIAL(fatal) <<__FILE__ << " [" << __FUNCTION__ << "] "


using std::string;

int random_candidate_expire();

int random_rv_retry_expire();

int random_ae_retry_expire();

unsigned smaller(unsigned a, unsigned b);

bool file_exists(const string &path);

string server2str(const std::tuple<string, int> &server);

std::tuple<string, int> get_peer_ip_port(std::shared_ptr<boost::asio::ip::tcp::socket> peer);



std::vector<string> split_str_boost(const string &s, char delim);

#endif //RAFT_UTIL_H
