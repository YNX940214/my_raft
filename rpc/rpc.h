#pragma once

#include <string>
#include <boost/asio.hpp>
#include <functional>
#include <map>
#include "../rpc.pb.h"
#include "rpc_type.h"

using std::string;
using boost::asio::ip::tcp;
using namespace boost::asio;
using namespace raft_rpc;

class RaftServer;

class connection;

using std::tuple;

typedef std::function<void(RPC_TYPE, string, tuple<string, int> addr)> incoming_rpc_callback;

class RPC {
public:
    RPC(boost::asio::io_context &io, const tcp::endpoint &endpoint, incoming_rpc_callback cb);

    void startAccept();

    void make_rpc_call(RPC_TYPE rpc_type, const std::tuple<string, int> &server, const string &rpc_msg);

    std::shared_ptr<connection> get(const tuple<string, int> &remote_peer);

    void insert(const tuple<string, int> &remote_peer, std::shared_ptr<connection> connection);

    void remove(const tuple<string, int> &remote_peer);

    void process_msg(char *data, int bytes_transferred, tuple<string, int> remote_peer);

private:
    const string msg_encode(const string &msg);

private:
    enum {
        max_body_length = 1024 * 5
    };
    boost::asio::io_context &io_;
    std::map<std::tuple<string, int>, std::shared_ptr<connection>> connection_map_;
    incoming_rpc_callback cb_;
    char big_char[max_body_length];
    tcp::acceptor _acceptor; //acceptor和接收的逻辑其实可以分离，但是accept的connection可以存到连接池里
};
