//
// Created by ynx on 2020-02-05.
//

#include "client.h"
#include "../rpc/rpc_type.h"
#include "../util.h"
#include <iostream>

using namespace std;

client::client(io_service &ioContext, const std::tuple<string, int> &server, StateMachine *sm) :
        ioContext_(ioContext),
        server_(server),
        sm_(sm),
        network_(ioContext, server_, std::bind(&client::react_to_resp, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)) {
    Log_info << "client's primary is set to " << server2str(server);

}

void client::set_server(std::tuple<string, int> server) {
    Log_info << "client's primary is set to " << server2str(server);
    server_ = server;
}

void client::run() {
    std::thread t([this]() { ioContext_.run(); });
    while (1) {
        cout << "input 'a' to apply or 'q' for query" << endl;
        char c;
        cin >> c;
        if (c == 'a') {
            apply_to_server();
        } else if (c == 'q') {
            query_from_server();
        }
    }
}


void client::apply_to_server() {
    string key;
    int v;
    cout << "input the key(string): " << endl;
    getline(cin, key);
    cout << "input the v(int): " << endl;
    cin >> v;
    const string &apply_str = sm_->build_apply_str(key, v);
    network_.make_rpc_call(CLIENT_APPLY, server_, apply_str);

}

void client::query_from_server() {
    cout << "input the key(string): " << endl;
    string key;
    getline(cin, key);
    const string &query_str = sm_->build_query_str(key);
    network_.make_rpc_call(CLIENT_QUERY, server_, query_str);
}

void client::react_to_resp(RPC_TYPE rpc_type, const string &resp_str, std::shared_ptr<tcp::socket> socket) {
    if (rpc_type == RESP_CLIENT_APPLY) {
        react_to_resp_query(resp_str);
    } else if (rpc_type == RESP_CLIENT_QUERY) {
        react_to_resp_apply(resp_str);
    } else {
        throw std::logic_error("unknown rpc_type");
    }
}

void client::react_to_resp_query(const string &resp_query_str) {
    Log_trace << "begin";
    sm_->react_to_resp_query(resp_query_str);
}

void client::react_to_resp_apply(const string &resp_apply_str) {
    Log_trace << "begin";
    sm_->react_to_resp_apply(resp_apply_str);
}