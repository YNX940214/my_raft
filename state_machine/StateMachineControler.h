//
// Created by ynx on 2020-02-01.
//

#ifndef RAFT_STATEMACHINECONTROLER_H
#define RAFT_STATEMACHINECONTROLER_H

#include "StateMachine.h"
#include "../entry/entry.h"
#include "ThreadPool.h"
#include <map>
#include <boost/asio.hpp>

class instance;

using std::map;

class StateMachineControler {
    /*
     * 目前实现的是只有一个worker线程，因为state_machine在多线程下，不能保证顺序。
     */
public:
    StateMachineControler(StateMachine *_stateMachine, Entries *_entries, map<unsigned int, std::shared_ptr<tcp::socket>> &_client_sockets_map, boost::asio::io_service &_io_service, int _read_thread_num = 5, int _write_thread_num = 1);

    void update_commit_index_and_apply(unsigned int new_index);

    void get_from_state_machine(const string &encoded_query_str, std::shared_ptr<tcp::socket> client_socket);

private:
    void apply_to_state_machine(unsigned int index_to_apply);

    void post_res_back_to_client(std::shared_ptr<tcp::socket> client_socket_sp, const string &res_str, unsigned int client_map_key);

    void callback_write_back_to_client(const boost::system::error_code &error, std::size_t bytes_transferred, std::shared_ptr<tcp::socket> client_socket_sp, unsigned int client_map_key);

private:
    StateMachine *state_machine_;
    Entries *entries_;
    unsigned int applied_index_;
    unsigned int commit_index_;  //这个变量是在io线程中
    ThreadPool write_thread_pool_;  // https://blog.csdn.net/caoshangpa/article/details/80374651
    ThreadPool read_thread_pool; //write_thread_pool不能有多个是因为 state_machine不支持多线程apply，但是read_thread_pool是支持多线程查询的
    map<unsigned int, std::shared_ptr<tcp::socket>> &client_sockets_map_;
    boost::asio::io_service &io_service_;
};


#endif //RAFT_STATEMACHINECONTROLER_H
