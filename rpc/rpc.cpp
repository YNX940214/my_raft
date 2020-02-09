#include "rpc.h"
#include "../log/boost_lop.h"
#include "../util.h"
#include "../RaftServer.h"
#include "rpc_to_string.h"

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
        auto sp = sockets_map_.get(server);
        if (sp) {
            add_header_then_write_and_hook(sp, rpc_msg, server);
        } else {
            string ip = std::get<0>(server);
            int port = std::get<1>(server);
            boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::address::from_string(ip), (unsigned short) port);
            sp = std::make_shared<tcp::socket>(io_);
            sp->async_connect(endpoint, [rpc_msg, sp, server, this](const boost::system::error_code &error) {
                Log_trace << "begin handler in RPC::writeTo's async_connect, error: " << error.message();
                if (error) {
                    Log_error << "async_connect, error: " << error.message();
                } else {
                    Log_trace << "async_connect to: " << server2str(get_socket_remote_ip_port(sp));
                    sockets_map_.insert(sp);
                    add_header_then_write_and_hook(sp, rpc_msg, server);
                }
            });
        }
    } catch (std::exception &exception) {
        Log_error << "exception: " << exception.what();
    }
}


//思考了一下，accept并不需要将新链接放入连接池，因为实现起来有困难。
void RPC::startAccept() {
    Log_trace << "begin";
    _acceptor.async_accept([this](const boost::system::error_code &error, tcp::socket peer) {
        auto sp = std::make_shared<tcp::socket>(std::move(peer));
        this->accept_callback(error, sp);
    });
}

void RPC::accept_callback(const boost::system::error_code &error, std::shared_ptr<tcp::socket> peer) {
    Log_trace << "begin, error: " << error.message() << ", peer: " << server2str(get_socket_remote_ip_port(peer));
    if (error) {
        Log_error << "error: " << error.message();
    } else {
        auto ep = peer->remote_endpoint();
        sockets_map_.insert(peer);
        read_header(peer);
    }
    startAccept();
}

void RPC::read_header(std::shared_ptr<tcp::socket> peer) {
    Log_trace << "begin";
    boost::asio::async_read(*peer, boost::asio::buffer(meta_char, 4), [this, peer](const boost::system::error_code &error, size_t bytes_transferred) {
        this->read_body(peer, error, bytes_transferred);
    });
}

void RPC::read_body(std::shared_ptr<tcp::socket> peer, const boost::system::error_code &error, size_t bytes_transferred) {
    Log_trace << "begin: error: " << error.message() << ", bytes_transferred: " << bytes_transferred << ", peer: " << server2str(get_socket_remote_ip_port(peer));
    if (error) {
        Log_error << "error: " << error.message();
        if (error == boost::asio::error::eof) {
            Log_info << "connection from " << peer->remote_endpoint().address() << ":" << peer->remote_endpoint().port() << " is closed that side";
            sockets_map_.remove(peer);
        } else {
            throw std::logic_error(error.message());  //这是完全有可能的，比如对面关闭了进程，需要处理
        }
    } else {
        char char_msg_len[5] = "";
        memcpy(char_msg_len, meta_char, 4);
        int msg_len = atoi(char_msg_len);
        boost::asio::async_read(*peer, boost::asio::buffer(big_char, msg_len), [this, peer](const boost::system::error_code &error, size_t bytes_transferred) {
            this->body_callback(peer, error, bytes_transferred);
        });
        read_header(peer);
    }
}


void RPC::body_callback(std::shared_ptr<tcp::socket> peer, const boost::system::error_code &error, size_t bytes_transferred) {
    Log_trace << "begin" << ", peer: " << server2str(get_socket_remote_ip_port(peer));
    if (error) {
        Log_error << "error: " << error.message();
        sockets_map_.remove(peer);
        throw std::logic_error(error.message());
    } else {
        unsigned int len_type = 1;
        char char_rpc_type[1] = "";
        memcpy(char_rpc_type, big_char, len_type);
        RPC_TYPE remote_rpc_type = static_cast<RPC_TYPE>(atoi(char_rpc_type));
        string msg(big_char + len_type, bytes_transferred - len_type);//纯的msg，不包括rpc type，比如已知rpc_type为Resp_AppendEntryRPC，那么msg的内容为{ ok=true, term =10}
        Log_debug << "received rpc: " << rpc_to_str(remote_rpc_type, msg);
        cb_(remote_rpc_type, msg, peer);
    }
}