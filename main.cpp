#include "RaftServer.h"
#include "state_machine/EasyStateMachine.h"
#include "state_machine/StateMachine.h"

int main(int argc, char **argv) {
    try {
        int _port = atoi(argv[2]);
        string config_path = string(argv[1]);
        init_logging(_port);
        boost::asio::io_service io;
        StateMachine *easy_state_machine = new EasyStateMachine();
        RaftServer raft_RaftServer(io, "127.0.0.1", _port, config_path, easy_state_machine);
        raft_RaftServer.run();
    } catch (std::exception &exception) {
        Log_fatal << "exception: " << exception.what();
    }
    return 0;
}