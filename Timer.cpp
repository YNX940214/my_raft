//
// Created by ynx on 2020-01-29.
//

#include "Timer.h"
#include "log/boost_lop.h"
#include "../raft/util.h"


Timer::Timer(boost::asio::io_service &io_service) : timer_(io_service) {
    Log_debug << "Timer " << &timer_ << " constructed";
}

Timer::~Timer() {
    Log_debug << "Timer " << &timer_ << " deconstructed";
}

boost::asio::deadline_timer &Timer::get_core() {
    return timer_;
}