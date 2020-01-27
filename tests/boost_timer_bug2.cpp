#define BOOST_ASIO_ENABLE_HANDLER_TRACKING

#include <iostream>
#include <vector>
#include <string>
#include <boost/asio.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>


namespace logging = boost::log;
namespace keywords = boost::log::keywords;
namespace attrs = boost::log::attributes;

using namespace boost::asio;
using namespace std;
using boost::asio::ip::tcp;


int main() {
    boost::asio::io_service io_context;
    boost::asio::deadline_timer t1(io_context);
    boost::asio::deadline_timer t2(io_context);
    boost::asio::deadline_timer t3(io_context);

    t1.expires_from_now(boost::posix_time::seconds(2));
    t1.async_wait([&](const boost::system::error_code &ec) {
        int num = t2.cancelation();
        cout << num << "handlers is canceled on t2" << endl;
    });

    t2.expires_from_now(boost::posix_time::seconds(3));
    t2.async_wait([&](const boost::system::error_code &ec) {
        if (ec) {
            cout << ec.message() << endl;
        } else {
            cout << "t2 expires with no error" << endl;
        }
    });

    t3.expires_from_now(boost::posix_time::seconds(5)); //如果换成5会得到完全不同的结果，这就是我在实现raft的时候遇到的问题的原因，即官方文档中说的无法取消一个已经fired的handler
    t3.wait();
    io_context.run();
    return 0;
}