//
// Created by ynx on 2020-02-05.
//

#include "EasyStateMachine.h"
#include "../util.h"
#include "../rpc/rpc_to_string.h"
#include <iostream>

using std::cout;
using std::endl;
using namespace easy_state_machine;

string EasyStateMachine::get(const string &encoded_get_str) {
    query_call call;
    call.ParseFromString(encoded_get_str);
    const string &key = call.key();

    resp_query resp;
    try {
        int v = mock_state_machine_[key];
        resp.set_ok(true);
        resp.set_v(v);
    } catch (const std::exception &ex) {
        resp.set_ok(false);
        resp.set_v(0);
    }
    string resp_str;
    resp.SerializeToString(&resp_str);
    return resp_str;
}

string EasyStateMachine::apply(const string &encoded_apply_str) {
    apply_call call;
    call.ParseFromString(encoded_apply_str);
    string key = call.key();
    int v = call.v();

    resp_apply resp;
    try {
        mock_state_machine_[key] = v;
        resp.set_ok(true);
    } catch (const std::exception &ex) {
        resp.set_ok(false);
    }
    string resp_str;
    resp.SerializeToString(&resp_str);
    return resp_str;
}

string EasyStateMachine::build_apply_str(const string &key, int v) {
    apply_call call;
    call.set_key(key);
    call.set_v(v);
    string res;
    call.SerializeToString(&res);
    return res;
}

string EasyStateMachine::build_query_str(const string &key) {
    query_call call;
    call.set_key(key);
    string res;
    call.SerializeToString(&res);
    return res;
}

void EasyStateMachine::react_to_resp_query(const string &resp_query) {
    Log_info << "client received resp_query: " << rpc_to_str(RESP_CLIENT_QUERY, resp_query);
    cout << "client received resp_query: " << rpc_to_str(RESP_CLIENT_QUERY, resp_query) << endl;
}

void EasyStateMachine::react_to_resp_apply(const string &resp_apply) {
    Log_info << "client received resp_apply: " << rpc_to_str(RESP_CLIENT_APPLY, resp_apply);
    cout << "client received resp_apply: " << rpc_to_str(RESP_CLIENT_APPLY, resp_apply) << endl;
}