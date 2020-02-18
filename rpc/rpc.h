#pragma once

#include <string>
#include <boost/asio.hpp>
#include <functional>
#include <map>
#include "../rpc.pb.h"
#include "rpc_type.h"
#include "SocketMap.h"

using std::string;
using boost::asio::ip::tcp;
using namespace boost::asio;
using namespace raft_rpc;

class RaftServer;

typedef std::function<void(RPC_TYPE, string, std::shared_ptr<tcp::socket>)> incoming_rpc_callback;

class connection;

class RPC {
public:
    RPC(boost::asio::io_context &io, const tcp::endpoint &endpoint, incoming_rpc_callback cb);

    void startAccept();

    void make_rpc_call(RPC_TYPE rpc_type, const std::tuple<string, int> &server, const string &rpc_msg);

private:
    void accept_callback(const boost::system::error_code &error, std::shared_ptr<tcp::socket> peer);

    void read_header(std::shared_ptr<tcp::socket> peer);

    void read_body(std::shared_ptr<tcp::socket> peer, const boost::system::error_code &error, size_t msg_len);

    void body_callback(std::shared_ptr<tcp::socket> peer, const boost::system::error_code &error, size_t bytes_transferred);

    void add_header_then_write_and_hook(std::shared_ptr<tcp::socket> sp, const string &rpc_msg, const std::tuple<string, int> &server);

private:
    enum {
        max_body_length = 1024 * 5
    };
//    std::function<void(boost::system::error_code &ec, std::size_t)> write_cb_; //for now, we dont' need it
    boost::asio::io_context &io_;
    std::map<std::tuple<string, int>, std::shared_ptr<connection>> connection_map_;
    incoming_rpc_callback cb_;
    char big_char[max_body_length];
    char meta_char[4];
    tcp::acceptor _acceptor; //acceptor和接收的逻辑其实可以分离，但是accept的connection可以存到连接池里
};
