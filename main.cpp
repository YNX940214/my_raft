#include "RaftServer.h"

int main(int argc, char **argv) {
    try {
        int _port = atoi(argv[2]);
        string config_path = string(argv[1]);
        init_logging(_port);
        boost::asio::io_service io;
        tcp::endpoint _endpoint(tcp::v4(), _port);
        RaftServer raft_RaftServer(io, "127.0.0.1", _port, _endpoint, config_path);
        raft_RaftServer.run();
    } catch (std::exception &exception) {
        Log_fatal << "exception: " << exception.what();
    }
    return 0;
}