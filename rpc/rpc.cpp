#include "rpc.h"
#include "../log/boost_lop.h"

string __get_ip_from_socket__(tcp::socket &peer) {
    tcp::endpoint endpoint = peer.remote_endpoint();
    boost::asio::ip::address address = endpoint.address();
    string ip = address.to_string();
    return ip;
}

RPC::RPC(boost::asio::io_context &io, std::function<void(RPC_TYPE, string, string, int)> cb) : _acceptor(io), io(io), cb(cb) {
}

void RPC::writeTo(string ip, int port, string rpc_msg, std::function<void(boost::system::error_code &ec, std::size_t)> cb) {
    try {
        //如果网络不可达到，tcp建立链接不能成功
        getConnection(ip, port, cb);
    } catch () { //catch boost tcp connection failed

    } catch (std::exception &exception) {
        BOOST_LOG_TRIVIAL(error) << "unconsidered exception: " + exception.what();
    }
}

//思考了一下，accept并不需要将新链接放入连接池，因为实现起来有困难。
void RPC::startAccept() {
    _acceptor.async_accept(std::bind(&RPC::accept_callback, this, std::placeholders::_1, std::placeholders::_2));
}

void RPC::accept_callback(const boost::system::error_code &error, tcp::socket peer) {
    if (error) {
        BOOST_LOG_TRIVIAL(error) << error;
    } else {
        read_header(std::move(peer));
    }
    startAccept();
}

void RPC::read_header(tcp::socket peer) {
    boost::asio::async_read(peer, boost::asio::buffer(meta_char, 4), std::bind(&RPC::read_body, this, std::move(peer), std::placeholders::_1, std::placeholders::_2));
}

void RPC::read_body(tcp::socket peer, const boost::system::error_code &error, size_t bytes_transferred) {
    if (error) {
        BOOST_LOG_TRIVIAL(error) << error;

        //出错
//        peer. cancel() or close()
        // 取消 能tm的取消么
    } else {
        char char_msg_len[5] = "";
        memcpy(char_msg_len, meta_char, 4);
        int msg_len = atoi(char_msg_len);
        //这里有个问题，我已经把socket move给了body_callback，还怎么再move 给read_head？所以我应该拿出信息，传给body_callback？
        boost::asio::async_read(peer, boost::asio::buffer(big_char, msg_len), std::bind(&RPC::body_callback, this, std::move(peer), std::placeholders::_1, std::placeholders::_2));
        read_header(std::move(peer));
    }
}

void RPC::body_callback(tcp::socket peer, const boost::system::error_code &error, size_t bytes_transferred) {
    if (error) {
        BOOST_LOG_TRIVIAL(error) << error;
//        peer.cancel() or close()?
    } else {
        char char_rpc_type[5] = "";
        memcpy(char_rpc_type, big_char, 4);
        int remote_rpc_type = atoi(char_rpc_type);
        //todo 能否更合理的转换
        if (remote_rpc_type == 1) {
            rpc_type = REQUEST_VOTE;
        } else if (remote_rpc_type == 2) {
            rpc_type = APPEND_ENTRY;
        } else if (remote_rpc_type == 3) {
            rpc_type = RESP_VOTE;
        } else if (remote_rpc_type == 4) {
            rpc_type = RESP_APPEND;
        } else {
            rpc_type = ERROR;
        }
        string msg(big_char + 4, bytes_transferred - 4); //纯的msg，不包括rpc type，比如已知rpc_type为Resp_AppendEntryRPC，那么msg的内容为{ ok=true, term =10}
        cb(rpc_type, msg);
    }
}

//void RPC::getConnection(string ip, int port, std::functio) {
//    如果是从pool中
//}