//
// Created by ynx on 2020-02-01.
//

#include "StateMachineControler.h"
#include "../util.h"

StateMachineControler::StateMachineControler(StateMachine *_stateMachine, Entries *_entries, map<unsigned int, std::shared_ptr<tcp::socket>> &_client_sockets_map, boost::asio::io_service &_io_service, int _read_thread_num, int _write_thread_num) :
        state_machine_(_stateMachine),
        entries_(_entries),
        applied_index_(-1),
        commit_index_(-1),
        write_thread_pool_(_write_thread_num),
        read_thread_pool(_read_thread_num),
        client_sockets_map_(_client_sockets_map),
        io_service_(_io_service) {
    if (_write_thread_num != 1) {
        throw std::logic_error("for now, the state machine doesn't support multithread apply, so the write thread pool num can only be 1");
    }
}

//调用上级是 react2resp_ae，不能直接感知到client的socket
void StateMachineControler::update_commit_index_and_apply(unsigned int new_index) { //这个对象是io线程调用的，所以在没有使用线程池的时候还是单线程的
    Log_trace << "begin, new_index: " << new_index;
    Log_debug << "StateMachineControler updated commit_index from " << commit_index_ << " to " << new_index;
    commit_index_ = new_index;
    while (applied_index_ < commit_index_) {
        apply_to_state_machine(applied_index_ + 1);
    }
}

void StateMachineControler::apply_to_state_machine(unsigned int index_to_apply) {
    // remember that index_to_apply is 1 bigger than applied_index
    Log_trace << "pushing entry of index " << index_to_apply << "'s state machine msg to write_thread_pool";
    const rpc_Entry &entry = entries_->get(index_to_apply);
    const string &encoded_apply_str = entry.msg();
    write_thread_pool_.enqueue([this, encoded_apply_str, index_to_apply]() {//注意这列必须复制这个_applied_index
        const string &sm_apply_res_str = this->state_machine_->apply(encoded_apply_str);
        auto client_socket_sp = this->client_sockets_map_[index_to_apply];
        this->post_res_back_to_client(client_socket_sp, sm_apply_res_str, index_to_apply);
    });
    applied_index_ = index_to_apply;
}


//调用上级是 接收到client的请求，能直接感知到client的socket
void StateMachineControler::get_from_state_machine(const string &encoded_query_str, std::shared_ptr<tcp::socket> client_socket) {
    Log_trace << " pushing client query to thread pool to find out the state machine";
    read_thread_pool.enqueue([this, encoded_query_str, client_socket] {
        const string &sm_query_res_str = this->state_machine_->get(encoded_query_str);
        this->post_res_back_to_client(client_socket, sm_query_res_str, -1);
    });
}

void StateMachineControler::post_res_back_to_client(std::shared_ptr<tcp::socket> client_socket_sp, const string &res_str, unsigned socket_map_key) {
    /*
     * apply_to_state_machine 和 get_from_state_machine 共用这个函数返回，区别是apply_to_state_machine
     * 需要传入entry index以从client_map中移除（因为apply是两条链，所以必须保存一个映射关系），而get的调用过程
     * 是可以一条链完成的，所以没有保存映射关系，对于get，传入socket_map_key -1一表示区别
     *
     */
    Log_trace << "post to ioContext to write the response to client: " << server2str(get_peer_ip_port(client_socket_sp));
    io_service_.post([client_socket_sp, res_str]() {
        boost::asio::async_write(*client_socket_sp, boost::asio::buffer(res_str), std::bind(&StateMachineControler::callback_write_back_to_client, std::placeholders::_1, std::placeholders::_2, client_socket_sp, client_map_key));
    });
}


void StateMachineControler::callback_write_back_to_client(const boost::system::error_code &error, std::size_t bytes_transferred, std::shared_ptr<tcp::socket> client_socket_sp, unsigned int client_map_key) {
    Log_trace << "called with error: " << error.message() << ", peer: " << server2str(get_peer_ip_port(client_socket_sp));
    if (!error) {
        Log_trace << "succeeded";
    } else {
        if (client_map_key != -1) {
            Log_debug << "error: " << error.message() << "remove socket from client_map with key: " << client_map_key;
            client_sockets_map_.erase(client_map_key);
        } else {
            //shared_ptr should release the socket
        }
    }
}