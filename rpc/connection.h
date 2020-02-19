//
// Created by ynx on 2020-02-18.
//

#ifndef RAFT_CONNECTION_H
#define RAFT_CONNECTION_H

#include <iostream>
#include <string>
#include <boost/asio.hpp>

using std::string;
using boost::asio::ip::tcp;
using std::tuple;

class RPC;

class connection : public std::enable_shared_from_this<connection> {
private:
    RPC &rpc_;
    tcp::socket socket_;
    string remote_addr_;
    int remote_port_;
    tuple<string, int> remote_peer_;

    string local_addr_;
    int local_port_;
public:
    //passive make connection
    connection(tcp::socket socket, RPC &rpc);

    //positive make connection
    connection(const tuple<string, int> &remote_server_addr, boost::asio::io_service &ioContext, RPC &rpc);

    void connect(std::function<void()> cb, const string &msg);

    ~connection();

    void start();

    void deliver(const string &msg);

    tuple<string, int> remote_addr();

    string remote_addr_str();

    string local_addr_str();

private:
    void read_header();

    void read_body(const boost::system::error_code &error, size_t msg_len);

    void body_callback(const boost::system::error_code &error, size_t bytes_transferred);

    char header_[4];
    char data_[1024 * 5];

};


#endif //RAFT_CONNECTION_H
