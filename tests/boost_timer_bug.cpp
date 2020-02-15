#define BOOST_ASIO_ENABLE_HANDLER_TRACKING

#include <iostream>
#include <vector>
#include <string>
#include <boost/asio.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <chrono>

namespace logging = boost::log;
namespace keywords = boost::log::keywords;
namespace attrs = boost::log::attributes;

using namespace boost::asio;
using namespace std;
using boost::asio::ip::tcp;


std::tuple<string, int> another_server("127.0.0.1", 7777);

class instance {
public:
    instance(io_service &loop, const string &_ip, int _port) :
            server_(_ip, _port),
            ioContext(loop),
            candidate_timer_(loop),
            strand_(boost::asio::make_strand(loop)),
            retry_timer_(loop) {
    }

    void run() {
        trans2C();
        ioContext.run();
    }


private:
    void trans2C() {
        BOOST_LOG_TRIVIAL(error) << "trans to candidate";
        cancel_all_timers();
        int waiting_count = candidate_timer_.expires_from_now(boost::posix_time::milliseconds(300));
        if (waiting_count == 0) {
        } else {
            throw_line("trans2C只能是timer_candidate_expire自然到期触发，所以不可能有waiting_count不为0的情况，否则就是未考虑的情况");
        }
        candidate_timer_.async_wait(boost::asio::bind_executor(strand_, [this](const boost::system::error_code &error) {
            if (error == boost::asio::error::operation_aborted) {
            } else {
                trans2C();
            }
        }));
        RV(another_server);
    }


    void RV(const tuple<string, int> &server) {
        BOOST_LOG_TRIVIAL(trace) << " send rpc_rv to server ";
        // https://stackoverflow.com/questions/43168199/cancelling-boost-asio-deadline-timer-safely
        //通过把timer的expire时间重设为0，并在handler调用时检验来判断是否被取消

        auto last_expire_time = retry_timer_.expires_at();
        int waiting_counts = retry_timer_.expires_from_now(boost::posix_time::milliseconds(10));
        cout << "定时器到期时间是否等于最小时间？ " << (last_expire_time == boost::posix_time::min_date_time) << " waiting_counts:" << waiting_counts;
        if (last_expire_time == boost::posix_time::min_date_time) {
            //canell_all_timers is called, so the retry should be canceled (no longer need to hook retry handler), just return
            return;
        }
        if (waiting_counts != 0) {
            std::ostringstream oss;
            oss << "if the cancel_all_timers has already canceled the RV retry, the exe stream will not come to here, as it comes here, there is something wrong" << endl;
            string s = oss.str();
            throw_line(s);
        } else {
            retry_timer_.async_wait(boost::asio::bind_executor(strand_, [this, server](const boost::system::error_code &error) {
                BOOST_LOG_TRIVIAL(trace) << "rv retry_timers expires";
                if (error == boost::asio::error::operation_aborted) {
                    BOOST_LOG_TRIVIAL(error) << "rv retry callback error: " << error.message();
                } else {
                    RV(server);
                }
            }));
        }
    }


private:
    void cancel_all_timers() {
        BOOST_LOG_TRIVIAL(trace) << "[BEGIN] cancel_all_timers ";
        int canceled_number = candidate_timer_.cancel();
        {
            std::ostringstream oss;
            oss << canceled_number << " handlers was canceled on the candidate_timer_" << endl;
            string s = oss.str();
            BOOST_LOG_TRIVIAL(debug) << s;
        }
        retry_timer_.expires_at(boost::posix_time::min_date_time);  //通过把timer的expire时间重设为0，并在handler调用时检验来判断是否被取消
        BOOST_LOG_TRIVIAL(trace) << "[DONE] cancel_all_timers";
    }

    io_context &ioContext;
    std::tuple<string, int> server_;
    deadline_timer candidate_timer_;
    deadline_timer retry_timer_;
    boost::asio::strand<boost::asio::io_context::executor_type> strand_;
};

int main() {
    try {
        boost::asio::io_service io;
        int _port = 8888;
        instance raft_instance(io, "127.0.0.1", _port);
        raft_instance.run();
    } catch (std::exception &exception) {
        BOOST_LOG_TRIVIAL(error) << " exception: " << exception.what();
    }
    return 0;
}