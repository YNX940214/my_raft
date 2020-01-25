#include <iostream>
#include <vector>
#include <string>
#include <boost/asio.hpp>
#include "util.h"
#include <iostream>
#include "../log/boost_lop.h"

using namespace boost::asio;
using namespace std;
using boost::asio::ip::tcp;
enum State {
    follower, candidate, primary
};

class instance {
public:
    instance(io_service &loop, const string &_ip, int _port, const tcp::endpoint &_endpoint) :
            ip_(_ip),
            port_(_port),
            server_(_ip, _port),
            ioContext(loop),
            candidate_timer_(loop),
            network_(loop, _endpoint, std::bind(&instance::reactToIncomingMsg, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)),
//            state_machine_(),
    {
        load_config_from_file();
        for (const auto &server_tuple: configuration_) {
            deadline_timer timer(loop);
            auto sp = make_shared<deadline_timer>(std::move(timer));
            retry_timers_.insert({server_tuple, sp});
        }
    }

    //mock 一下先
    void load_config_from_file() {
        configuration_ = {
//                {"127.0.0.1", 8888},
                {"127.0.0.1", 7777},
//                {"127.0.0.1", 9999},
        };
    }

    void run() {
        network_.startAccept();
        int waiting_counts = candidate_timer_.expires_from_now(boost::posix_time::milliseconds(random_candidate_expire()));
        if (waiting_counts != 0) {
            throw std::logic_error(" not  0");
        } else {
            candidate_timer_.async_wait([this](const boost::system::error_code &error) {
                if (error == boost::asio::error::operation_aborted) {
                } else {
                    trans2C();
                }
            });
        }
        ioContext.run();
    }

    void cancel_all_timers() {
        BOOST_LOG_TRIVIAL(trace) << server2str(server_) << " all timer canceled";
        candidate_timer_.cancel();
        for (auto pair : retry_timers_) {
            pair.second->cancel();
        }
    }

private:
    void trans2C() {
        BOOST_LOG_TRIVIAL(error) << server2str(server_) << "trans to candidate";
        cancel_all_timers();
        int waiting_count = candidate_timer_.expires_from_now(boost::posix_time::milliseconds(random_candidate_expire()));
        if (waiting_count == 0) {
        } else {
            throw std::logic_error("trans2C只能是timer_candidate_expire自然到期触发，所以不可能有waiting_count不为0的情况，否则就是未考虑的情况");
        }
        candidate_timer_.async_wait([this](const boost::system::error_code &error) {
            if (error == boost::asio::error::operation_aborted) {
            } else {
                trans2C();
            }
        });
        for (auto &server : configuration_) {
            int port = std::get<1>(server);
            if (port != port_) {
                RV(server);
            }
        }
    }


    void RV(const tuple<string, int> &server) {
        BOOST_LOG_TRIVIAL(trace) << server2str(server_) << " send rpc_rv to server " << server2str(server);
        shared_ptr<deadline_timer> timer = retry_timers_[server];
        int waiting_counts = timer->expires_from_now(boost::posix_time::milliseconds(random_rv_retry_expire()));
        if (waiting_counts != 0) {
            std::ostringstream oss;
            oss << "rv retry timer is set and hooks is not zero, it should be " << server2str(server);
            string s = oss.str();
            throw logic_error(s);
        } else {
            timer->async_wait([this, server](const boost::system::error_code &error) {
                BOOST_LOG_TRIVIAL(trace) << "rv retry_timers expires" << server2str(server);
                if (error == boost::asio::error::operation_aborted) {
                    BOOST_LOG_TRIVIAL(error) << server2str(server_) << "rv retry callback error: " << error.message();
                } else {
                    RV(server);
                }
            });
        }
    }

    void writeTo(tuple<string, int> server, string msg) {
        network_.writeTo(server, msg, std::bind(&instance::deal_with_write_error, this, std::placeholders::_1, std::placeholders::_2));
    }

private:
    io_context &ioContext;
    vector<std::tuple<string, int>> configuration_;
    //rpc
    tcp::endpoint endpoint_;
    string ip_;
    int port_;
    std::tuple<string, int> server_;
    RPC network_;

    //timers
    deadline_timer candidate_timer_;
    map<std::tuple<string, int>, shared_ptr<deadline_timer> > retry_timers_;
};

int main() {
    try {
        init_logging();
        boost::asio::io_service io;
        int _port = 8888;
        tcp::endpoint _endpoint(tcp::v4(), _port);
        instance raft_instance(io, "127.0.0.1", _port, _endpoint);
        raft_instance.run();
    } catch (std::exception &exception) {
        BOOST_LOG_TRIVIAL(error) << " exception: " << exception.what();
    }
    return 0;
}