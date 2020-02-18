#include "rpc.h"
#include "../log/boost_lop.h"
#include "../util.h"
#include "../RaftServer.h"
#include "rpc_to_string.h"
#include "connection.h"

using std::cout;
using std::endl;
using namespace raft_rpc;

RPC::RPC(boost::asio::io_context &io, const tcp::endpoint &endpoint, incoming_rpc_callback cb) : io_(io), _acceptor(io, endpoint), cb_(cb) {
}


inline void RPC::add_header_then_write_and_hook(std::shared_ptr<tcp::socket> sp, const string &msg, const std::tuple<string, int> &peer) {
    Log_trace << "begin";
    char header[4 + 1] = "";
    int msg_len = msg.length();
    std::sprintf(header, "%4d", static_cast<int>(msg_len));
    char send_buffer[msg_len + 4];
    memcpy(send_buffer, header, 4);
    memcpy(send_buffer + 4, msg.c_str(), msg_len);

    boost::asio::async_write(*sp, boost::asio::buffer(send_buffer, msg_len + 4), [this, peer](const boost::system::error_code &error, std::size_t bytes_transferred) {
        Log_trace << "[begin] handler in add_header_then_write_and_hook exipired, error: " << error.message();
        if (error) {
            Log_error << "handler in RPC::add_header_then_write_and_hook's async_write failed, error: " << error.message();
            sockets_map_.remove(peer);
        } else {
            Log_trace << "handler in RPC::add_header_then_write_and_hook's async_write succeeded, error: " << error.message();
        }
    });
}


void RPC::make_rpc_call(RPC_TYPE rpc_type, const std::tuple<string, int> &server, const string &_rpc_msg) {
    Log_debug << "making rpc to server " << server2str(server) << ", detail: " << rpc_to_str(rpc_type, _rpc_msg);
    string rpc_msg = std::to_string(rpc_type) + _rpc_msg;
    try {
        auto sp = get(server);
        string tosend = build_string();
        if (!sp) {
            sp->deliver(tosend);
        } else {
            connection con(server, io_, *this);
            con.connect(std::bind(to_send));
        }
    } catch (std::exception &exception) {
        Log_error << "exception: " << exception.what();
    }
}


//思考了一下，accept并不需要将新链接放入连接池，因为实现起来有困难。
void RPC::startAccept() {
    Log_trace << "begin";
    _acceptor.async_accept([this](const boost::system::error_code &error, tcp::socket peer) {
        if (error) {
            Log_error << "accept error: " << error.message();
        } else {
            std::make_shared<connection>(std::move(peer), *this)->start();

        }
        startAccept();
    });
}

std::shared_ptr<connection> RPC::get(const tuple<string, int> &remote_peer) {
    auto sp = connection_map_[remote_peer];
    if (!sp) {
        Log_debug << "socket to " << server2str(remote_peer) << " is null";
    } else {
        Log_debug << "socket to " << server2str(remote_peer) << " is not null, local endpoint" << sp->local_addr_str();
    }
    return sp;
}

void RPC::insert(const tuple<string, int> &remote_peer, std::shared_ptr<connection> connection) {
    Log_trace << "inserting connection, remote:" << connection->remote_addr_str() << ", local: " << connection->local_addr_str();
    if (connection_map_.find(remote_peer) != connection_map_.end()) {
        Log_error << "inserting  key " << server2str(remote_peer) << " already in the map, that's an manager error";
        throw_line("insert an exsting key, check log");
    }
    connection_map_[remote_peer] = connection;
}

void RPC::remove(const tuple<string, int> &remote_peer) {
    Log_trace << "removing connection, remote:" << server2str(remote_peer);
    if (connection_map_.find(remote_peer) == connection_map_.end()) {
        Log_error << "can't find key " << server2str(remote_peer) << " in the map, that's an manager error";
        throw_line("can't find key in map, check log");
    }
    connection_map_.erase(remote_peer);
}

void RPC::process_msg(char *data, int bytes_transferred, tuple<string, int> remote_peer) {
    Log_trace << "begin, remote_peer: " << server2str(remote_peer);
    unsigned int len_type = 1;
    char char_rpc_type[1] = "";
    memcpy(char_rpc_type, big_char, len_type);
    RPC_TYPE remote_rpc_type = static_cast<RPC_TYPE>(atoi(char_rpc_type));
    string msg(data + len_type, bytes_transferred - len_type); //纯的msg，不包括rpc type，比如已知rpc_type为Resp_AppendEntryRPC，那么msg的内容为{ ok=true, term =10}
    Log_debug << "received rpc: " << rpc_to_str(remote_rpc_type, msg);
    cb_(remote_rpc_type, msg, remote_peer);
}