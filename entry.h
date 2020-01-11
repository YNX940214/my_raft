//
// Created by ynx on 2019-12-17.
//

#ifndef RAFT_ENTRY_H
#define RAFT_ENTRY_H

#include <string>
#include <vector>
#include "rpc/rpc.h"
#include "rpc.pb.h"

using std::vector;
using std::string;
using raft_rpc::rpc_Entry;


class Entries {
public:
    rpc_Entry get(int index);

    unsigned get_commit_index();

    void insert(unsigned prelog_index,rpc_Entry &entry); //同步接口,自动根据插入位置插入，并删除插入位置之后的entry

    void update_commit_index(unsigned remote_commit_index, unsigned remote_prelog_index);

    rpc_Entry &last_entry();

    unsigned size();

    const rpc_Entry &operator[](unsigned i) const;

private:
    unsigned _commit_index;
    unsigned tail_index;
    vector<rpc_Entry> entries;
};

#endif //RAFT_ENTRY_H
