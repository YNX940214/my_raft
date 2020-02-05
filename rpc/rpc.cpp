#include "rpc.h"
#include "../log/boost_lop.h"
#include "../util.h"

using std::cout;
using std::endl;
using namespace raft_rpc;

RPC::RPC(boost::asio::io_context &io, const tcp::endpoint &endpoint, std::function<void(RPC_TYPE, string)> cb) : io_(io), _acceptor(io, endpoint), cb_(cb) {
}


inline void RPC::add_header_then_write_and_hook(std::shared_ptr<tcp::socket> sp, const string &msg, const std::tuple<string, int> &server) {
    Log_trace << "begin";
    char header[4 + 1] = "";
    int msg_len = msg.length();
    std::sprintf(header, "%4d", static_cast<int>(msg_len));
    char send_buffer[msg_len + 4];
    memcpy(send_buffer, header, 4);
    memcpy(send_buffer + 4, msg.c_str(), msg_len);

    boost::asio::async_write(*sp, boost::asio::buffer(send_buffer, msg_len + 4), [this, server](const boost::system::error_code &error, std::size_t bytes_transferred) {
        Log_trace << "[begin] handler in add_header_then_write_and_hook exipired, error: " << error.message();
        if (error) {
            Log_error << "handler in RPC::add_header_then_write_and_hook's async_write failed, error: " << error.message();
            client_sockets_.erase(server);
        } else {
            Log_trace << "handler in RPC::add_header_then_write_and_hook's async_write succeeded, error: " << error.message();
        }
    });
    Log_trace << "done";
}

string rpc_ae2str(const AppendEntryRpc &ae) {
    std::ostringstream oss;
    oss << "RPC_AE:\n"
           "term: " << ae.term()
        << "\nprelog_term: " << ae.prelog_term()
        << "\ncommit_index: " << ae.commit_index()
        << "\nlsn: " << ae.lsn()
        << "\nip: " << ae.ip()
        << "\nport: " << ae.port()
        << "\nae.rpc_entry:";
    if (ae.entry_size() != 0) {
        oss << "\n\tterm: " << ae.entry(0).term()
            << "\n\tindex: " << ae.entry(0).index()
            << "\n\tmsg: " << ae.entry(0).msg();
    } else {
        oss << "empty";
    }
    string s = oss.str();
    return s;
}

string rpc_rv2str(const RequestVoteRpc &rv) {
    std::ostringstream oss;
    oss << "RPC_RV:\n"
           "term: " << rv.term()
        << "\nlatest_index: " << rv.latest_index()
        << "\nlatest_term: " << rv.latest_term()
        << "\nlsn: " << rv.lsn()
        << "\nip: " << rv.ip()
        << "\nport: " << rv.port();
    string s = oss.str();
    return s;
}

string resp_ae2str(const Resp_AppendEntryRpc &resp) {
    std::ostringstream oss;
    oss << "resp_ae:\n"
           "ok: " << resp.ok()
        << "\nterm: " << resp.term()
        << "\nlsn: " << resp.lsn()
        << "\nip: " << resp.ip()
        << "\nport: " << resp.port();
    string s = oss.str();
    return s;
}


string resp_rv2str(const Resp_RequestVoteRpc &resp) {
    std::ostringstream oss;
    oss << "resp_rv:\n"
           "ok: " << resp.ok()
        << "\nterm: " << resp.term()
        << "\nlsn: " << resp.lsn()
        << "\nip: " << resp.ip()
        << "\nport: " << resp.port();
    string s = oss.str();
    return s;
}

string rpc_to_str(RPC_TYPE type, const string &rpc_str) {
    switch (type) {
        case REQUEST_VOTE: {
            RequestVoteRpc rpc;
            rpc.ParseFromString(rpc_str);
            return rpc_rv2str(rpc);
        }
            break;
        case APPEND_ENTRY: {
            AppendEntryRpc rpc;
            rpc.ParseFromString(rpc_str);
            return rpc_ae2str(rpc);
        }
            break;
        case RESP_VOTE: {
            Resp_RequestVoteRpc rpc;
            rpc.ParseFromString(rpc_str);
            return resp_rv2str(rpc);
        }
            break;
        case RESP_APPEND: {
            Resp_AppendEntryRpc rpc;
            rpc.ParseFromString(rpc_str);
            return resp_ae2str(rpc);
        }
            break;
        case CLIENT_APPLY:
            break;
        case CLIENT_QUERY:
            break;
        case RESP_CLIENT_APPLY:
            break;
        case RESP_CLIENT_QUERY:
            break;
        default:
            throw std::runtime_error("unknown rpc type");
    }
}


add header
//to_string(APPEND_ENTRY) + to_string(REQUEST_VOTE) +
void RPC::make_rpc_call(RPC_TYPE rpc_type, const std::tuple<string, int> &server, const string &rpc_msg, std::function<void(boost::system::error_code &ec, std::size_t)> cb) {
    Log_debug << "making rpc to server " << server2str(server) << ", detail: " << rpc_to_str(rpc_)
    try {
        Log_debug << "geting server " << server2str(server) << "'s tcp sp";
        auto sp = client_sockets_[server];
        if (sp) {
            Log_debug << "sp debug: sp is NOT null, server: " << server2str(server) << "local endpoint: " << sp->local_endpoint().address() << ":" << sp->local_endpoint().port();
            add_header_then_write_and_hook(sp, rpc_msg, server);
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
                    add_header_then_write_and_hook(sp, rpc_msg, server);
                }
            });
        }
    } catch (std::exception &exception) {
        Log_error << "unconsidered exception: " << exception.what();
    }
    Log_trace << "done";
}

//void RPC::make_rpc_call(RPC_TYPE rpc_type, std::shared_ptr<tcp::socket> sp_socket, const string &rpc_msg, std::function<void(boost::system::error_code &ec, std::size_t)> cb) {
//    Log_trace << "begin";
//    try {
//        Log_debug << "geting server " << server2str(server) << "'s tcp sp";
//        auto sp = client_sockets_[server];
//        if (sp) {
//            Log_debug << "sp debug: sp is NOT null, server: " << server2str(server) << "local endpoint: " << sp->local_endpoint().address() << ":" << sp->local_endpoint().port();
//            add_header_then_write_and_hook(sp, rpc_msg, server);
//        } else {
//            Log_debug << "sp debug: sp is null";
//
//            string ip = std::get<0>(server);
//            int port = std::get<1>(server);
//            boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::address::from_string(ip), (unsigned short) port);
//            sp = std::make_shared<tcp::socket>(io_);
//            sp->async_connect(endpoint, [rpc_msg, sp, server, this](const boost::system::error_code &error) {
//                Log_trace << "begin handler in RPC::writeTo's async_connect, error: " << error.message();
//                if (error) {
//                    Log_error << "async_connect, error: " << error.message();
//                } else {
//                    Log_trace << "async_connect to: " << sp->remote_endpoint().address() << ":" << sp->remote_endpoint().port() << " local: " << sp->local_endpoint().address() << ":" << sp->local_endpoint().port();
//                    client_sockets_[server] = sp;
//                    Log_trace << "insert happened: " << server2str(server) << " with local endpoint: " << sp->local_endpoint().address() << ":" << sp->local_endpoint().port();
////                    int rp = client_sockets_[server]->remote_endpoint().port();
//                    add_header_then_write_and_hook(sp, rpc_msg, server);
//                }
//            });
//        }
//    } catch (std::exception &exception) {
//        Log_error << "unconsidered exception: " << exception.what();
//    }
//    Log_trace << "done";
//}


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
    Log_trace << "begin, error: " << error.message() << ", peer: " << server2str(get_peer_ip_port(peer));
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
        Log_trace << "begin: handler in read_header's async_read called, error: " << error.message() << ", peer: " << server2str(get_peer_ip_port(peer));
        this->read_body(peer, error, bytes_transferred);
    });
}

void RPC::read_body(std::shared_ptr<tcp::socket> peer, const boost::system::error_code &error, size_t bytes_transferred) {
    Log_trace << "begin: error: " << error.message() << ", bytes_transferred: " << bytes_transferred << ", peer: " << server2str(get_peer_ip_port(peer));
    if (error) {
        Log_error << "error: " << error.message();
        if (error == boost::asio::error::eof) {
            Log_info << "connection from " << peer->remote_endpoint().address() << ":" << peer->remote_endpoint().port() << " is closed that side";
        } else {
            throw std::logic_error(error.message());  //这是完全有可能的，比如对面关闭了进程，需要处理
        }
    } else {
        char char_msg_len[5] = "";
        memcpy(char_msg_len, meta_char, 4);
        int msg_len = atoi(char_msg_len);
        boost::asio::async_read(*peer, boost::asio::buffer(big_char, msg_len), [this, peer](const boost::system::error_code &error, size_t bytes_transferred) {
            Log_debug << "handler in read_body's async_read called, error: " << error.message() << ", peer: " << server2str(get_peer_ip_port(peer));
            this->body_callback(peer, error, bytes_transferred);
        });
        read_header(peer);
    }
}


void RPC::body_callback(std::shared_ptr<tcp::socket> peer, const boost::system::error_code &error, size_t bytes_transferred) {
    Log_trace << "begin" << ", peer: " << server2str(get_peer_ip_port(peer));
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
        cb_(remote_rpc_type, msg, peer);
    }
}