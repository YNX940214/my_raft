//
// Created by ynx on 2019-11-19.
//

#ifndef RAFT_UTIL_H
#define RAFT_UTIL_H

#include "util.h"
#include <iostream>
#include <string>
#include "rpc.pb.h"
#include <sstream>
#include <boost/asio.hpp>

#define DEBUG_FACTOR 1;
using std::string;


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

std::tuple<string, int> get_peer_server_tuple(std::shared_ptr<boost::asio::ip::tcp::socket> peer) {
    auto remote_ep = peer->remote_endpoint();
    return std::make_tuple(remote_ep.address().to_string(), remote_ep.port());
}

string rpc_ae2str(const raft_rpc::AppendEntryRpc &ae) {
    std::ostringstream oss;
    oss << "RPC_AE:\n"
           "term: " << ae.term()
        << "\nprelog_term: " << ae.prelog_term()
        << "\ncommit_index: " << ae.commit_index()
        << "\nlsn: " << ae.lsn()
        << "\nip: " << ae.ip()
        << "\nport: " << ae.port()
        << "\nae.rpc_entry:";
    if (ae.entry_size() != 0) {
        oss << "\n\tterm: " << ae.entry(0).term()
            << "\n\tindex: " << ae.entry(0).index()
            << "\n\tmsg: " << ae.entry(0).msg();
    } else {
        oss << "empty";
    }
    string s = oss.str();
    return s;
}

string rpc_rv2str(const raft_rpc::RequestVoteRpc &rv) {
    std::ostringstream oss;
    oss << "RPC_RV:\n"
           "term: " << rv.term()
        << "\nlatest_index: " << rv.latest_index()
        << "\nlatest_term: " << rv.latest_term()
        << "\nlsn: " << rv.lsn()
        << "\nip: " << rv.ip()
        << "\nport: " << rv.port();
    string s = oss.str();
    return s;
}

string resp_ae2str(const raft_rpc::Resp_AppendEntryRpc &resp) {
    std::ostringstream oss;
    oss << "resp_ae:\n"
           "ok: " << resp.ok()
        << "\nterm: " << resp.term()
        << "\nlsn: " << resp.lsn()
        << "\nip: " << resp.ip()
        << "\nport: " << resp.port();
    string s = oss.str();
    return s;
}


string resp_rv2str(const raft_rpc::Resp_RequestVoteRpc &resp) {
    std::ostringstream oss;
    oss << "resp_rv:\n"
           "ok: " << resp.ok()
        << "\nterm: " << resp.term()
        << "\nlsn: " << resp.lsn()
        << "\nip: " << resp.ip()
        << "\nport: " << resp.port();
    string s = oss.str();
    return s;
}

#endif //RAFT_UTIL_H
