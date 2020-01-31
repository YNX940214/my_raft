#include "rpc.h"
#include "../log/boost_lop.h"
#include "../util.h"

using std::cout;
using std::endl;

string __get_ip_from_socket__(tcp::socket &peer) {
    tcp::endpoint endpoint = peer.remote_endpoint();
    boost::asio::ip::address address = endpoint.address();
    string ip = address.to_string();
    return ip;
}

RPC::RPC(boost::asio::io_context &io, const tcp::endpoint &endpoint, std::function<void(RPC_TYPE, string)> cb) : io_(io), _acceptor(io, endpoint), cb_(cb) {
}


inline void RPC::add_header_then_write_and_hook(std::shared_ptr<tcp::socket> sp, const string &msg) {
    Log_trace << "begin";
    char header[4 + 1] = "";
    int msg_len = msg.length();
    std::sprintf(header, "%4d", static_cast<int>(msg_len));
    char send_buffer[msg_len + 4];
    memcpy(send_buffer, header, 4);
    memcpy(send_buffer + 4, msg.c_str(), msg_len);

    boost::asio::async_write(*sp, boost::asio::buffer(send_buffer, msg_len + 4), [](const boost::system::error_code &error, std::size_t bytes_transferred) {
        Log_trace << "[begin] handler in add_header_then_write_and_hook exipired, error: " << error.message();
        //write succeed, do nothing
        Log_trace << "[done] handler in add_header_then_write_and_hook exipired, error: " << error.message();
    });
    Log_trace << "done";
}

void RPC::writeTo(const std::tuple<string, int> &server, const string &rpc_msg, std::function<void(boost::system::error_code &ec, std::size_t)> cb) {
    Log_trace << "begin";
    try {
        Log_debug << "geting server " << server2str(server) << "'s tcp sp";
        auto sp = client_sockets_[server];
        if (sp) {
            Log_debug << "sp debug: sp is NOT null, server: " << server2str(server) << "local endpoint: " << sp->local_endpoint().address() << ":" << sp->local_endpoint().port();
            add_header_then_write_and_hook(sp, rpc_msg);
        } else {
            Log_debug << "sp debug: sp is null";

            string ip = std::get<0>(server);
            int port = std::get<1>(server);
            boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::address::from_string(ip), (unsigned short) port);
            sp = std::make_shared<tcp::socket>(io_);
            sp->async_connect(endpoint, [rpc_msg, sp, server, this](const boost::system::error_code &error) {
                Log_trace << "begin handler in RPC::writeTo's async_connect, error: " << error.message();
                if (error) {
                    Log_error << "async_connect, error: " << error.message();
                } else {
                    Log_trace << "async_connect to: " << sp->remote_endpoint().address() << ":" << sp->remote_endpoint().port() << " local: " << sp->local_endpoint().address() << ":" << sp->local_endpoint().port();
                    client_sockets_[server] = sp;
                    Log_trace << "insert happened: " << server2str(server) << " with local endpoint: " << sp->local_endpoint().address() << ":" << sp->local_endpoint().port();
//                    int rp = client_sockets_[server]->remote_endpoint().port();
                    add_header_then_write_and_hook(sp, rpc_msg);
                }
            });
        }
    } catch (std::exception &exception) {
        Log_error << "unconsidered exception: " << exception.what();
    }
    Log_trace << "done";
}

tcp::socket RPC::getConnection(std::string ip, int port, std::function<void(boost::system::error_code &ec, std::size_t)> cb) {
    //make is simple for now, i just wanna compile and build, let's make connection pool later
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
    Log_trace << "begin, error: " << error.message() << ", ip: " << peer->remote_endpoint().address() << ", port: " << peer->remote_endpoint().port();
    if (error) {
        Log_error << "error: " << error.message();
    } else {
        auto ep = peer->remote_endpoint();
        cout << ep.address() << ep.port() << endl;
        read_header(peer);
    }
    startAccept();
}

void RPC::read_header(std::shared_ptr<tcp::socket> peer) {
    Log_trace << "begin";
    boost::asio::async_read(*peer, boost::asio::buffer(meta_char, 4), [this, peer](const boost::system::error_code &error, size_t bytes_transferred) {
        std::tuple<string, int> server = get_peer_server_tuple(peer);
        Log_trace << "begin: handler in read_header's async_read called, error: " << error.message() << ", ip: " << peer->remote_endpoint().address() << ", port: " << peer->remote_endpoint().port();
        this->read_body(peer, error, bytes_transferred);
    });
}

void RPC::read_body(std::shared_ptr<tcp::socket> peer, const boost::system::error_code &error, size_t bytes_transferred) {
    Log_trace << "begin: error: " << error.message() << ", bytes_transferred: " << bytes_transferred << ", ip: " << peer->remote_endpoint().address() << ", port: " << peer->remote_endpoint().port();
    if (error) {
        Log_error << "error: " << error.message();
        if (error == boost::asio::error::eof) {
            Log_info << "connection from " << peer->remote_endpoint().address() << ":" << peer->remote_endpoint().port() << " is closed that side";
        }else{
            throw std::logic_error(error.message());  //这是完全有可能的，比如对面关闭了进程，需要处理
        }
    } else {
        char char_msg_len[5] = "";
        memcpy(char_msg_len, meta_char, 4);
        int msg_len = atoi(char_msg_len);
        boost::asio::async_read(*peer, boost::asio::buffer(big_char, msg_len), [this, peer](const boost::system::error_code &error, size_t bytes_transferred) {
            Log_debug << "handler in read_body's async_read called, error: " << error.message() << ", ip: " << peer->remote_endpoint().address() << ", port: " << peer->remote_endpoint().port();
            this->body_callback(peer, error, bytes_transferred);
        });
        read_header(peer);
    }
}


void RPC::body_callback(std::shared_ptr<tcp::socket> peer, const boost::system::error_code &error, size_t bytes_transferred) {
    Log_trace << "begin" << ", ip: " << peer->remote_endpoint().address() << ", port: " << peer->remote_endpoint().port();
    if (error) {
        Log_error << "error: " << error.message();
//        peer.cancel() or close()?
    } else {
        unsigned int len_type = 1;
        char char_rpc_type[1] = "";
        memcpy(char_rpc_type, big_char, len_type);
        RPC_TYPE remote_rpc_type = static_cast<RPC_TYPE>(atoi(char_rpc_type));
        Log_debug << "body_callback received msg's rpc_type is: " << remote_rpc_type;
        string msg(big_char + len_type, bytes_transferred - len_type);//纯的msg，不包括rpc type，比如已知rpc_type为Resp_AppendEntryRPC，那么msg的内容为{ ok=true, term =10}
        cb_(remote_rpc_type, msg);
    }
}