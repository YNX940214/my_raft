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