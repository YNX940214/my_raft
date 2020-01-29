#pragma once

#include <string>
#include <boost/asio.hpp>
#include <functional>
#include <map>

using std::string;
using boost::asio::ip::tcp;
using namespace boost::asio;
enum RPC_TYPE {
    REQUEST_VOTE, APPEND_ENTRY, RESP_VOTE, RESP_APPEND, ERROR
};

class RPC {
public:
    RPC(boost::asio::io_context &io, const tcp::endpoint &endpoint, std::function<void(RPC_TYPE, string)> cb);

    void writeTo(std::tuple<string, int> server, string rpc_msg, std::function<void(boost::system::error_code &ec, std::size_t)> cb); //cb为callback，在RPC::writeTo中根据成功/失败执行下一步动作
    void startAccept();

private:

    boost::asio::io_context &io_;

    tcp::socket getConnection(std::string ip, int port, std::function<void(boost::system::error_code &ec, std::size_t)> cb); //异步的获取connection，需要传入回调

    void accept_callback(const boost::system::error_code &error, std::shared_ptr<tcp::socket> peer);

    void read_header(std::shared_ptr<tcp::socket> peer);

    void read_body(std::shared_ptr<tcp::socket> peer, const boost::system::error_code &error, size_t msg_len);

    void body_callback(std::shared_ptr<tcp::socket> peer, const boost::system::error_code &error, size_t bytes_transferred);

    void add_header_then_write_and_hook(std::shared_ptr<tcp::socket> sp, const string &rpc_msg);

public:

private:
    enum {
        max_body_length = 1024 * 5
    };

    std::map<std::tuple<string, int>, std::shared_ptr<tcp::socket>> client_sockets_;
    std::function<void(RPC_TYPE, string msg)> cb_;
    char big_char[max_body_length];
    char meta_char[4];
//    std::map<std::tuple<string, int>, std::shared_ptr<tcp::socket>> _connection_map;
    tcp::acceptor _acceptor; //acceptor和接收的逻辑其实可以分离，但是accept的connection可以存到连接池里
};
