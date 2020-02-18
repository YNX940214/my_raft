//
// Created by ynx on 2020-02-18.
//

#include "connection.h"
#include "../util.h"
#include "rpc.h"

connection::connection(const tuple<string, int> &remote_server_addr, boost::asio::io_service &ioContext, RPC rpc) :
        socket_(ioContext),
        rpc_(rpc) {
    remote_addr_ = std::get<0>(remote_server_addr);
    remote_port_ = std::get<1>(remote_server_addr);
    remote_peer_ = std::make_tuple(remote_addr_, remote_port_);
}

void connection::connect(std::function<void()> cb) {
    auto self = shared_from_this();
    boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::address::from_string(remote_addr_), (unsigned short) remote_port_);
    socket_.async_connect(endpoint, [cb, self](const boost::system::error_code &error) {
        Log_trace << "async connect, error: " << error.message();
        if (error) {
            Log_error << "async_connect, error: " << error.message();
        } else {
            Log_info << "connected to: " << self->remote_addr_str() << ", local: " << self->local_addr_str(); //
            self->read_header();
            rpc_.insert(self->remote_peer_, self);
            cb();
        }
    });

}

connection::connection(tcp::socket socket, RPC &rpc) :
        socket_(std::move(socket)),
        rpc_(rpc) {
    try {
        const auto &remote_peer = socket_.remote_endpoint();
        remote_addr_ = remote_peer.address().to_string();
        remote_port_ = remote_peer.port();
        remote_peer_ = std::make_tuple(remote_addr_, remote_port_);

        const auto &local_peer = socket_.local_endpoint();
        local_addr_ = local_peer.address().to_string();
        local_port_ = local_peer.port();
        Log_info << "connection between local " << local_addr_ << ":" << local_port_ << " and remote " << remote_addr_ << ":" << remote_port_ << "constructed";
    } catch (std::exception &exp) {
        Log_error << "error happend when constructing an connection: " << exp.what();
    }
}

connection::~connection() {
    Log_info << "connection between local " << local_addr_ << ":" << local_port_ << " and remote " << remote_addr_ << ":" << remote_port_ << "detroyed";
}


void connection::start() {
    Log_trace << "begin";
    auto sp = shared_from_this();
    rpc_.insert(remote_peer_, sp);
    read_header();
}

void connection::read_header() {
    Log_trace << "begin, remote: " << remote_addr_str() << ", local: " << local_addr_str();
    auto sp = shared_from_this();
    boost::asio::async_read(socket_, boost::asio::buffer(header_, 4), [sp](const boost::system::error_code &error, size_t bytes_transferred) {
        sp->read_body(error, bytes_transferred);
    });
}

void connection::read_body(const boost::system::error_code &error, size_t bytes_transferred) {
    Log_trace << "begin, error: " << error.message() << ", bytes: " << bytes_transferred;
    if (error) {
        if (error == boost::asio::error::eof) {
            Log_error << "error: " << error.message();
            rpc_.remove(remote_peer_);
        } else {
            throw_line(error.message()); //这是完全有可能的，比如对面关闭了进程，需要处理
        }
    } else {
        auto sp = shared_from_this();
        char char_msg_len[5] = "";
        memcpy(char_msg_len, header_, 4);
        int msg_len = atoi(char_msg_len);
        boost::asio::async_read(socket_, boost::asio::buffer(data_, msg_len), [sp](const boost::system::error_code &error, size_t bytes_transferred) {
            sp->body_callback(error, bytes_transferred);
        });
        read_header();
    }
}

void connection::body_callback(const boost::system::error_code &error, size_t bytes_transferred) {
    Log_trace << "begin, error: " << error.message() << ", bytes: " << bytes_transferred;
    if (error) {
        throw_line("我们这里暂时让程序崩溃"); //这是完全有可能的，比如对面关闭了进程，需要处理
    } else {
        auto sp = shared_from_this();
        rpc_.process_msg(data_, bytes_transferred, sp->remote_peer_);
    }
}


void connection::deliver(const string &msg) {
    Log_trace << "begin, remote: " << remote_addr_str() << ", local: " << local_addr_str();
    boost::asio::async_write(socket_, boost::asio::buffer(msg), [this](const boost::system::error_code &error, std::size_t bytes_transferred) {
        Log_trace << "begin";
        if (error) {
            Log_error << "deliver failed, error: " << error.message();
            rpc_.remove(remote_peer_);
        } else {
            Log_trace << "succeed";
        }
    });
}

string connection::local_addr_str() {
    return local_addr_ + ":" + std::to_string(local_port_);
}

string connection::remote_addr_str() {
    return remote_addr_ + ":" + std::to_string(remote_port_);
}