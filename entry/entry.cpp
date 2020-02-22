//
// Created by ynx on 2019-12-17.
//

#include "entry.h"
#include "../log/boost_lop.h"
#include <stdexcept>
#include <sstream>
#include <fstream>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/serialization/vector.hpp>
#include "../consts.h"
#include "../rpc.pb.h"
#include <limits.h>
#include "../rpc/rpc_to_string.h"

using std::cout;
using std::cin;
using std::endl;
using std::string;
using std::logic_error;
using std::ifstream;
using std::ofstream;


//Entries::Entries(int port) : data_(string(Consts::data_path) + string(".") + std::to_string(port)) {
//Entries::Entries(int port) : data_(string(Consts::data_path) + string(".") + std::to_string(port)) {
//    path_offset_ = string(Consts::offset_path) + string(".") + std::to_string(port);
//    Log_debug << "调试阶段，每次都从头开始";
//    offset_.push_back(0);
//
//    size_ = offset_.size() - 1;
//    entries_.reserve(size_);
//    /*
//     * if data_ file exists, we need to rebuild entries_ from it and offset_
//     */
//    for (int i = 0; i < size_; i++) {
//        int entry_head = offset_[i];
//        int entry_tail = offset_[i + 1];
//        // read from file with [entry_head, entry_tail)
//        string data = data_.read_from_offset(entry_head, entry_tail - entry_head);
//        rpc_Entry entry;
//        entry.ParseFromString(data);
//        entries_.push_back(entry);
//    }
//}
Entries::Entries(int port) : data_(string(Consts::data_path) + string(".") + std::to_string(port)) {
    path_offset_ = string(Consts::offset_path) + string(".") + std::to_string(port);
    if (!file_exists(path_offset_.c_str())) {
        //needn't open the file, open it before flush
        offset_.push_back(0);
    } else {
        ifstream is(path_offset_, std::ios::binary);
        boost::archive::binary_iarchive iar(is);
        iar >> offset_;
    }
    size_ = offset_.size() - 1;
    cout << "load entries, entry size: " << size_ << endl;
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
        cout << "reload entry: " << entry2str(entry);
        entry.ParseFromString(data);
        entries_.push_back(entry);
    }
}

int Entries::size() {
    return size_;
}

const rpc_Entry &Entries::get(int index) {
    if (index >= size_) {
        std::ostringstream oss;
        oss << "the etries's size is " << size_ << ", the get index is " << index << ", which is bigger";
        string s = oss.str();
        throw_line(s);
    }
    return entries_[index];
}


void Entries::insert(unsigned int index, const rpc_Entry &entry) {
    Log_debug << "entry insert begin, index: " << index << ", entry: " << entry2str(entry);
    if (index > size_) {
        std::ostringstream oss;
        oss << "the etries's size is " << size_ << ", the insert index is " << index << ", which is bigger";
        string s = oss.str();
        throw_line(s);
    }

    unsigned int offset_insert = get_offset(index);
    string entry_string;
    entry.SerializeToString(&entry_string);
    data_.write_from_offset(offset_insert, entry_string);

    // write offset file
    modify_offset_vector_after_insert(index, entry_string);
    write_offset_to_file();

    //at last
    if (index == size_) {
        if (entries_.size() == index) {
            entries_.push_back(entry);
        } else {
            entries_[index] = entry;
        }
    } else if (index < size_) {
        entries_[index] = entry;//we don't release the memory because as if the system is running one day, the entries will grow to that length (without checkpoint)
    } else {
        std::ostringstream oss;
        oss << "insert error, size_ is " << size_ << ", index is " << index;
        string s = oss.str();
        throw_line(s);
    }
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
        throw_line(s);
    }
}

unsigned int Entries::get_offset(int index) {
    if (index >= offset_.size()) { // be careful with the structure of the entries, the last ele is the EOF offset
        std::ostringstream oss;
        oss << "trying to get the " << index << "th offset, there are " << size_ << " entries in entries";
        string s = oss.str();
        throw_line(s);
    } else {
        return offset_[index];
    }
}

void Entries::write_offset_to_file() {
    ofstream os(path_offset_, std::ios::binary);
    boost::archive::binary_oarchive oar(os);
    oar << offset_;
}


//int main() {
//    Entries _entries;
//    //it it amazing to find cin with so many bugs
//    while (1) {
//        cout << "i for insert and g for get" << endl;
//        char action;
//        cin >> action;
//        if (action == 'i') {
//            cout << "index to insert?" << endl;
//            int index;
//            cin >> index;
//            while (cin.fail()) {
//                cout << "you should enter an interger!\nindex to insert?" << endl;
//                cin.clear();
//                cin.ignore();
////                    cin.ignore(std::numeric_limits<std::streamsize>::max(), ' '); //quoting https://stackoverflow.com/questions/7413247/cin-clear-doesnt-reset-cin-object
//                cin >> index;
//            }
//            cin.ignore();   //https://stackoverflow.com/questions/17005725/c-cin-weird-behaviour
//            cout << "string to insert?" << endl;
//            string str;
//            std::getline(cin, str);
//            rpc_Entry entry;
//            entry.set_term(1);
//            entry.set_index(1);
//            entry.set_msg(str);
//            _entries.insert(index, entry);
//        }
//        if (action == 'g') {
//            cout << "index to get?" << endl;
//            int index;
//            cin >> index;
//            rpc_Entry entry = _entries.get(index);
//            cout << entry.msg();
//        }
//    }
//}