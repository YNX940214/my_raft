//
// Created by ynx on 2019-12-17.
//

#include "entry.h"
#include "util.h"
#include "log/boost_lop.h"

void Entries::update_commit_index(unsigned remote_commit_index, unsigned remote_prelog_index) {
    //todo  mutex
//    mutex and defer
    int smaller_index = smaller(remote_commit_index, remote_prelog_index);
    if (smaller_index > _commit_index) {
        _commit_index = smaller_index;
    } else if (smaller_index == _commit_index) {

    } else {
        string err = "remote_commit_index " + std::to_string(smaller_index) + " is smaller than local commit_index " + std::to_string(_commit_index) + ", this is impossible";
        BOOST_LOG_TRIVIAL(error) << err;
    }

}