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
    RPC(boost::asio::io_context &io, std::function<void(string)> cb);

    void writeTo(string ip, int port, string rpc_msg, std::function<void(bool)> cb); //cb为callback，在RPC::writeTo中根据成功/失败执行下一步动作
    void startAccept();

private:

    boost::asio::io_context &io;

    void getConnection(std::string ip, int port, std::function<void(bool)> cb); //异步的获取connection，需要传入回调


    void accept_callback(const boost::system::error_code &error, tcp::socket peer);

    void read_header(tcp::socket peer);

    void read_body(tcp::socket peer, const boost::system::error_code &error, size_t bytes_transferred);

    void body_callback(tcp::socket peer, const boost::system::error_code &error, size_t bytes_transferred);

public:

private:
    enum {
        max_body_length = 1024 * 5
    };


    RPC_TYPE rpc_type;
    std::function<void(RPC_TYPE, string)> cb;
    char big_char[max_body_length];
    char meta_char[8];
    std::map<string, tcp::socket> _connection_map;
    tcp::acceptor _acceptor; //acceptor和接收的逻辑其实可以分离，但是accept的connection可以存到连接池里
};
