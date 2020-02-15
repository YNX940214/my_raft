//
// Created by ynx on 2020-02-09.
//
#include "rpc_to_string.h"
#include "../state_machine/easy_state_machine.pb.h"
#include <sstream>
#include "../util.h"


using namespace easy_state_machine;

string rpc_ae2str(const AppendEntryRpc &ae) {
    std::ostringstream oss;
    oss << "RPC_AE:";
    if (ae.entry_size() != 0) {
        oss << "(with msg)";
    }

    oss << "\nterm: " << ae.term()
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

string rpc_rv2str(const RequestVoteRpc &rv) {
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

string resp_ae2str(const Resp_AppendEntryRpc &resp) {
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


string resp_rv2str(const Resp_RequestVoteRpc &resp) {
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

// 虽然raft server 不能知道statemachine具体的实现，但是这不是重点所以先写了
string apply_call2str(const apply_call &rpc) {
    std::ostringstream oss;
    oss << "apply_call:\n"
           "key: " << rpc.key()
        << "\nv: " << rpc.v();
    string s = oss.str();
    return s;
}

string resp_apply2str(const resp_apply &resp) {
    std::ostringstream oss;
    oss << "resp_apply:\n"
           "ok: " << resp.ok();
    string s = oss.str();
    return s;
}

string query_call2str(const query_call &rpc) {
    std::ostringstream oss;
    oss << "query_call:\n"
           "key: " << rpc.key();
    string s = oss.str();
    return s;
}

string resp_query2str(const resp_query &resp) {
    std::ostringstream oss;
    oss << "resp_query:\n"
           "ok: " << resp.ok()
        << "\nv: " << resp.v();
    string s = oss.str();
    return s;
}

string entry2str(const rpc_Entry &entry) {
    std::ostringstream oss;
    oss << "entry:\n"
           "term: " << entry.term()
        << "\nindex: " << entry.index()
        << "\nmsg: " << entry.msg();
    string s = oss.str();
    return s;
}

string rpc_to_str(RPC_TYPE type, const string &rpc_str) {
    switch (type) {
        case REQUEST_VOTE: {
            RequestVoteRpc rpc;
            rpc.ParseFromString(rpc_str);
            return rpc_rv2str(rpc);
        }
            break;
        case APPEND_ENTRY: {
            AppendEntryRpc rpc;
            rpc.ParseFromString(rpc_str);
            return rpc_ae2str(rpc);
        }
            break;
        case RESP_VOTE: {
            Resp_RequestVoteRpc rpc;
            rpc.ParseFromString(rpc_str);
            return resp_rv2str(rpc);
        }
            break;
        case RESP_APPEND: {
            Resp_AppendEntryRpc rpc;
            rpc.ParseFromString(rpc_str);
            return resp_ae2str(rpc);
        }
            break;
        case CLIENT_APPLY: {
            apply_call rpc;
            rpc.ParseFromString(rpc_str);
            return apply_call2str(rpc);
        }
            break;
        case CLIENT_QUERY: {
            query_call rpc;
            rpc.ParseFromString(rpc_str);
            return query_call2str(rpc);
        }
            break;
        case RESP_CLIENT_APPLY: {
            resp_apply rpc;
            rpc.ParseFromString(rpc_str);
            return resp_apply2str(rpc);
        }
            break;
        case RESP_CLIENT_QUERY: {
            resp_query rpc;
            rpc.ParseFromString(rpc_str);
            return resp_query2str(rpc);
        }
            break;
        default:
            throw_line("unknown rpc type");
    }
}
