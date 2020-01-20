//
// Created by ynx on 2019-12-17.
//

#include "entry.h"
#include "../util.h"
#include "../log/boost_lop.h"
#include <stdexcept>
#include <sstream>
#include <fstream>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/serialization/vector.hpp>
#include "../consts.h"
#include "../rpc.pb.h"

using std::cout, std::cin, std::endl, std::string;
using std::logic_error;
using std::ifstream;
using std::ofstream;

Entries::Entries() : data_(Consts::data_path) {
    path_offset_ = Consts::offset_path;
    if (!file_exists(path_offset_.c_str())) {
        //needn't open the file, open it before flush
        offset_.push_back(0);
    } else {
        ifstream is(path_offset_, std::ios::binary);
        boost::archive::binary_iarchive iar(is);
        iar >> offset_;
    }
    size_ = offset_.size() - 1;
    entries_.reserve(size_);
    /*
     * if data_ file exists, we need to rebuild entries_ from it and offset_
     */
    for (int i = 0; i < size_; i++) {
        int entry_head = offset_[i];
        int entry_tail = offset_[i + 1];
        // read from file with [entry_head, entry_tail)
        string data = data_.read_from_offset(entry_head, entry_tail - entry_head);
        rpc_Entry entry;
        entry.ParseFromString(data);
        entries_.push_back(entry);
    }
}

unsigned Entries::size() {
    return size_;
}

const rpc_Entry &Entries::get(int index) {
    if (index >= size_) {
        std::ostringstream oss;
        oss << "the etries's size is " << size_ << ", the get index size is " << index << ", which is bigger";
        string s = oss.str();
        throw logic_error(s);
    }
    return entries_[index];
}


void Entries::insert(unsigned int index, const rpc_Entry &entry) {
    if (index > size_) {
        std::ostringstream oss;
        oss << "the etries's size is " << size_ << ", the get index size is " << index << ", which is bigger";
        string s = oss.str();
        throw logic_error(s);
    }

    unsigned int offset_insert = get_offset(index);
    string entry_string;
    entry.SerializeToString(&entry_string);

    data_.write_from_offset(offset_insert, entry_string);

    // write offset file
    modify_offset_vector_after_insert(index, entry_string);
    write_offset_to_file();

    //at last
    entries_[index] = entry; //we don't release the memory because as if the system is running one day, the entries will grow to that length (without checkpoint)
    size_ = index + 1;
}

void Entries::modify_offset_vector_after_insert(int index, const string &entry_str) {
    if (index == offset_.size() - 1) {
        offset_.push_back(offset_[index] + entry_str.size());
    } else if (index < offset_.size() - 1) {
        offset_[index + 1] = offset_[index] + entry_str.size();
        offset_.erase(offset_.begin() + index + 2, offset_.end());
    } else {
        std::ostringstream oss;
        oss << "trying to modify offset with index " << index << " but there are only " << size_ << " offsets";
        string s = oss.str();
        throw logic_error(s);
    }
}

unsigned int Entries::get_offset(int index) {
    if (index >= offset_.size()) { // be careful with the structure of the entries, the last ele is the EOF offset
        std::ostringstream oss;
        oss << "trying to get the " << index << "th offset, there are " << size_ << " entries in entries";
        string s = oss.str();
        throw logic_error(s);
    } else {
        return offset_[index];
    }
}

void Entries::write_offset_to_file() {
    ofstream os(path_offset_, std::ios::binary);
    boost::archive::binary_oarchive oar(os);
    oar << offset_;
}


int main() {
    Entries _entries;
    while (1) {
        cout << "i for insert and g for get" << endl;
        char action;
        cin >> action;
        if (action == 'i') {
            cout << "index to insert?" << endl;
            int index;
            cin >> index;
            cout << "string to insert?" << endl;
            string str;
            cin >> str;
            rpc_Entry entry;
            entry.set_term(1);
            entry.set_index(1);
            entry.set_msg(str);
            _entries.insert(index, entry);
        }
        if (action == 'g') {
            cout << "index to get?" << endl;
            int index;
            cin >> index;
            rpc_Entry entry = _entries.get(index);
            cout << entry.msg();
        }
    }
}