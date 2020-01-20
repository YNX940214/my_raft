//
// Created by ynx on 2019-12-17.
//

#ifndef RAFT_ENTRY_H
#define RAFT_ENTRY_H

#include <string>
#include <vector>
#include "../rpc/rpc.h"
#include "../rpc.pb.h"
#include "data.h"

using std::vector;
using std::string;
using raft_rpc::rpc_Entry;


class Entries {
public:
    Entries();

    const rpc_Entry &get(int index);

    /*
     * we only write the disk with write op, for the read op, an in memory data structure is maintained
     * and we read it when read op happens, but if write op happends, we must write to the disk first,
     * after if succeeded, then can we modify the in memory data, then can we return
     */
    void insert(unsigned int index, const rpc_Entry &entry);

    unsigned int size();

private:
    void modify_offset_vector_after_insert(int index, const string &entry_str);

    unsigned int get_offset(int index);

    void write_offset_to_file();

private:
    data data_;
    vector<rpc_Entry> entries_;
    /*
     * stores the offset of the entries, the last value is end of the last entry +1
     * e.g.       |0|200|500| means there are two entry, A from 0 to 199, B from 200 to 499, 500 = 499 + 1
     *
     */
    vector<unsigned int> offset_; //
    unsigned int size_;
    string path_offset_;
    //unsigned _commit_index; after some thought, the commit_index will not be saved on disk, just a in memory value( i am not sure this is right)

};

#endif //RAFT_ENTRY_H
