//
// Created by ynx on 2020-02-09.
//

#ifndef RAFT_SOCKETMAP_H
#define RAFT_SOCKETMAP_H

#include <boost/asio.hpp>
#include <map>
#include <string>

using boost::asio::ip::tcp;
using std::string;

class SocketMap {
public:
    std::shared_ptr<tcp::socket> get(const std::tuple<string, int> &addr);

    void insert(std::shared_ptr<tcp::socket> sp);

    void remove(const std::tuple<string, int> &peer);

    void remove(std::shared_ptr<tcp::socket> peer);

private:
    std::map<std::tuple<string, int>, std::shared_ptr<tcp::socket>> map_;
};


#endif //RAFT_SOCKETMAP_H
