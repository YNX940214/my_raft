//
// Created by ynx on 2020-01-19.
//

#ifndef RAFT_DATA_H
#define RAFT_DATA_H

#include <string>

using std::string;

class data {
public:
    data(const string &path);

    void write_from_offset(int offset, const string & data);

    string read_from_offset(int offset, int len);

private:
    int file_fd_;
};


#endif //RAFT_DATA_H
