//
// Created by ynx on 2020-01-29.
//

#ifndef RAFT_TIMER_H
#define RAFT_TIMER_H

#include "boost/asio.hpp"

using namespace boost::asio;

//boost::asio的timer莫名其妙变成了null，但是我在初始化server的时候将timer的sp insert到了map，按理说怎么也不可能出现timer 析构的情况,所以价格class封装一下
class Timer {
public:
    Timer(boost::asio::io_service &io_service);

    ~Timer();

    boost::asio::deadline_timer &get_core();


private:
    boost::asio::deadline_timer timer_;

};


#endif //RAFT_TIMER_H
