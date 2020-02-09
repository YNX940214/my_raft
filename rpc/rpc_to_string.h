//
// Created by ynx on 2020-02-09.
//

#ifndef RAFT_RPC_TO_STRING_H
#define RAFT_RPC_TO_STRING_H

#include <string>
#include "../rpc.pb.h"
#include "rpc_type.h"

using namespace raft_rpc;
using std::string;

string rpc_ae2str(const AppendEntryRpc &ae);

string rpc_rv2str(const RequestVoteRpc &rv);

string resp_ae2str(const Resp_AppendEntryRpc &resp);

string resp_rv2str(const Resp_RequestVoteRpc &resp);

string rpc_to_str(RPC_TYPE type, const string &rpc_str);

#endif //RAFT_RPC_TO_STRING_H
