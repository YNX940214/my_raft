//
// Created by ynx on 2020-01-19.
//

#include "data.h"
#include <unistd.h>
#include <fcntl.h>
#include <sstream>
#include <stdexcept>
#include <iostream>

//data::data(const string &path) {
//    const char *path_char = path.c_str();
//    {
//        Log_debug << "为了方便调试，每次都创建文件";
//        if ((file_fd_ = open(path_char, O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) == -1) {
//            std::ostringstream oss;
//            oss << "创建出错";
//            string s = oss.str();
//            throw_line(s);
//        }
//    }
//}
data::data(const string &path) {
    const char *path_char = path.c_str();
    if (!file_exists(path_char)) {
        if ((file_fd_ = open(path_char, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) == -1) {
            std::ostringstream oss;
            oss << "文件不存在，但是创建出错";
            string s = oss.str();
            throw_line(s);
        }
    } else {
        if ((file_fd_ = open(path.c_str(), O_RDWR)) == -1) {
            std::ostringstream oss;
            oss << "文件存在，打开文件出错";
            string s = oss.str();
            throw_line(s);
        }
    }
}

string data::read_from_offset(int offset, int len) {
    Log_debug << "begin, offset: " << offset << ", len: " << len;
    lseek(file_fd_, offset, SEEK_SET);
    string res(len, 0);
    read(file_fd_, (void *) res.c_str(), len);
    std::cout << "loaded entry string size: " << res.size();
    return res;
}

void data::write_from_offset(int offset, const string &data) {
    Log_debug << "begin, offset: " << offset << "string size: " << data.size();
    lseek(file_fd_, offset, SEEK_SET);
    auto n_write = write(file_fd_, data.c_str(), data.size());
    if (n_write < 0) {
        std::cout << "errno: " << errno << std::endl;
        exit(EXIT_FAILURE);
    }
    std::cout << "n_write" << n_write << ", string size: " << data.size() << std::endl;
    ftruncate(file_fd_, offset + data.size());
    fsync(file_fd_);
}


//int main() {
//    data file_data("test111.txt");
//    file_data.write_from_offset(0, "0123456789");
//    std::cout << file_data.read_from_offset(1, 3);
//}