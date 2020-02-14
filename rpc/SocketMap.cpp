//
// Created by ynx on 2020-02-09.
//

#include "SocketMap.h"

#include "../util.h"

std::shared_ptr<tcp::socket> SocketMap::get(const std::tuple<string, int> &addr) {
    auto socket_sp = map_[addr];
    if (!socket_sp) {
        Log_debug << "socket to " << server2str(addr) << " is null";
    } else {
        Log_debug << "socket to " << server2str(addr) << " is not null, local endpoint" << server2str(get_socket_local_ip_port(socket_sp));
    }
    return socket_sp;
}

void SocketMap::insert(std::shared_ptr<tcp::socket> sp) {
    const auto &remote = get_socket_remote_ip_port(sp);
    const auto &local = get_socket_local_ip_port(sp);
    Log_trace << "inserting remote endpoint " << server2str(remote) << ", local endpoint " << server2str(local) << " into socket map";
    map_[remote] = sp;
}

void SocketMap::remove(const std::tuple<string, int> &addr) {
    Log_trace << "removing " << server2str(addr) << " from socket map";
    map_.erase(addr);
}

void SocketMap::remove(std::shared_ptr<tcp::socket> peer) {
    const auto &addr = get_socket_remote_ip_port(peer);
    Log_trace << "removing " << server2str(addr) << " from socket map";
    map_.erase(addr);
}