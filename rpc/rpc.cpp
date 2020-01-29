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
    char header[4 + 1] = "";
    int msg_len = msg.length();
    std::sprintf(header, "%4d", static_cast<int>(msg_len));
    char send_buffer[msg_len + 4];
    memcpy(send_buffer, header, 4);
    memcpy(send_buffer + 4, msg.c_str(), msg_len);

    boost::asio::async_write(*sp, boost::asio::buffer(send_buffer, msg_len + 4), [](const boost::system::error_code &error, std::size_t bytes_transferred) {
        Log_trace << "[begin] handler in add_header_then_write_and_hook exipired, error: "<<error.message();
        //write succeed, do nothing
        Log_trace << "[done] handler in add_header_then_write_and_hook exipired, error: "<<error.message();

    });
}

void RPC::writeTo(std::tuple<string, int> server, string rpc_msg, std::function<void(boost::system::error_code &ec, std::size_t)> cb) {
    try {
        auto sp = client_sockets_[server];
        if (sp) {
            add_header_then_write_and_hook(sp, rpc_msg);
        } else {
            string ip = std::get<0>(server);
            int port = std::get<1>(server);
            boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::address::from_string(ip), (unsigned short) port);
            sp = std::make_shared<tcp::socket>(io_);
            sp->async_connect(endpoint, [rpc_msg, sp, server, this](const boost::system::error_code &error) {
                BOOST_LOG_TRIVIAL(trace) << "[begin] writeTo async_connect handler: (error: " << error.message() << ")";
                if (error) {
                    BOOST_LOG_TRIVIAL(error) << "async_connect_error";
                } else {
                    client_sockets_.insert({server, sp});
                    add_header_then_write_and_hook(sp, rpc_msg);
                }
                BOOST_LOG_TRIVIAL(trace) << "[done] writeTo async_connect handler: (error: " << error.message() << ")";
            });
        }
    } catch (std::exception &exception) {
        BOOST_LOG_TRIVIAL(error) << "unconsidered exception: " << exception.what();
    }
}

tcp::socket RPC::getConnection(std::string ip, int port, std::function<void(boost::system::error_code &ec, std::size_t)> cb) {
    //make is simple for now, i just wanna compile and build, let's make connection pool later
}

//思考了一下，accept并不需要将新链接放入连接池，因为实现起来有困难。
void RPC::startAccept() {
    BOOST_LOG_TRIVIAL(trace) << " RPC::startAccept";
    _acceptor.async_accept([this](const boost::system::error_code &error, tcp::socket peer) {
        auto sp = std::make_shared<tcp::socket>(std::move(peer));
        this->accept_callback(error, sp);
    });
}

void RPC::accept_callback(const boost::system::error_code &error, std::shared_ptr<tcp::socket> peer) {
    if (error) {
        BOOST_LOG_TRIVIAL(error) << error;
    } else {
        auto ep = peer->remote_endpoint();
        cout << ep.address() << ep.port() << endl;
        read_header(peer);
    }
    startAccept();
}

//todo 这种形式居然不能用，不跟它死磕了，用lambda
//void RPC::read_header(tcp::socket peer) {
////    boost::asio::async_read(peer, boost::asio::buffer(meta_char, 4), std::bind(&RPC::read_body, this, std::move(peer), std::placeholders::_1, std::placeholders::_2));
////}

void RPC::read_header(std::shared_ptr<tcp::socket> peer) {
    boost::asio::async_read(*peer, boost::asio::buffer(meta_char, 4), [this, peer](const boost::system::error_code &error, size_t bytes_transferred) {
        BOOST_LOG_TRIVIAL(trace) << "[begin] read header handler: error: " << error.message();
        this->read_body(peer, error, bytes_transferred);
        BOOST_LOG_TRIVIAL(trace) << "[done] read header handler: error: " << error.message();

    });
}

void RPC::read_body(std::shared_ptr<tcp::socket> peer, const boost::system::error_code &error, size_t bytes_transferred) {
    BOOST_LOG_TRIVIAL(trace) << "[begin] RPC::read_body (error: " << error.message() << ", bytes_transferred: " << bytes_transferred << ")";
    if (error) {
        BOOST_LOG_TRIVIAL(error) << error.message();
        //出错
//        peer. cancel() or close()
        // 取消 能tm的取消么
    } else {
        char char_msg_len[5] = "";
        memcpy(char_msg_len, meta_char, 4);
        int msg_len = atoi(char_msg_len);
        //这里有个问题，我已经把socket move给了body_callback，还怎么再move 给read_head？所以我应该拿出信息，传给body_callback？
        boost::asio::async_read(*peer, boost::asio::buffer(big_char, msg_len), [this, &peer](const boost::system::error_code &error, size_t bytes_transferred) {
            this->body_callback(peer, error, bytes_transferred);
        });
        read_header(peer);
    }
    BOOST_LOG_TRIVIAL(trace) << "[done] RPC::read_body (error: " << error.message() << ", bytes_transferred: " << bytes_transferred << ")";

}

std::tuple<string, int> get_peer_server_tuple(std::shared_ptr<tcp::socket> peer) {
    auto remote_ep = peer->remote_endpoint();
    return std::make_tuple(remote_ep.address().to_string(), remote_ep.port());
}

void show_peer_remote_endpoint(std::shared_ptr<tcp::socket> peer) {
    const auto &server = get_peer_server_tuple(peer);
    cout << server2str(server);
}

void RPC::body_callback(std::shared_ptr<tcp::socket> peer, const boost::system::error_code &error, size_t bytes_transferred) {
    if (error) {
        BOOST_LOG_TRIVIAL(error) << error;
//        peer.cancel() or close()?
    } else {
        //todo 这里换成1byte了，但是代码仍然是4byte
        unsigned int len_type = 1;
        char char_rpc_type[1] = "";
        memcpy(char_rpc_type, big_char, len_type);
        RPC_TYPE remote_rpc_type = static_cast<RPC_TYPE>(atoi(char_rpc_type));

        std::tuple<string, int> server = get_peer_server_tuple(peer);
        string msg(big_char + len_type, bytes_transferred - len_type);//纯的msg，不包括rpc type，比如已知rpc_type为Resp_AppendEntryRPC，那么msg的内容为{ ok=true, term =10}
        cb_(remote_rpc_type, msg);
    }
}

//void RPC::getConnection(string ip, int port, std::functio) {
//    如果是从pool中
//}