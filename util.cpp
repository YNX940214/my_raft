//
// Created by ynx on 2019-11-19.
//

#include "util.h"
#include <iostream>
#include <string>
#include "rpc.pb.h"
#include <sstream>
#include <boost/asio.hpp>
#include <vector>
#include <boost/algorithm/string.hpp>


#define DEBUG_FACTOR 1;
using std::string;

std::vector<string> split_str_boost(const string &str, char delim) {
    std::vector<std::string> results;
    boost::split(results, str, [delim](char c) { return c == delim; });
    return results;
}

int random_candidate_expire() {
    int max = 300 * DEBUG_FACTOR;
    int min = 150 * DEBUG_FACTOR;
    int range = max - min + 1;
    int num = rand() % range + min;

    return num;
}

int random_rv_retry_expire() {
    int max = 10 * DEBUG_FACTOR;
    int min = 5 * DEBUG_FACTOR;
    int range = max - min + 1;
    int num = rand() % range + min;

    return num;
}

int random_ae_retry_expire() {
    int max = 10 * DEBUG_FACTOR;
    int min = 5 * DEBUG_FACTOR;
    int range = max - min + 1;
    int num = rand() % range + min;
    return num;
}

int smaller(int a, int b) {
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

std::tuple<string, int> get_socket_remote_ip_port(std::shared_ptr<boost::asio::ip::tcp::socket> peer) {
    //todo maybe more robust
    const auto &remote_ep = peer->remote_endpoint();
    return std::make_tuple(remote_ep.address().to_string(), remote_ep.port());
}

std::tuple<string, int> get_socket_local_ip_port(std::shared_ptr<boost::asio::ip::tcp::socket> peer) {
    const auto &local_ep = peer->local_endpoint();
    return std::make_tuple(local_ep.address().to_string(), local_ep.port());
}

