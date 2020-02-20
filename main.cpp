#include "RaftServer.h"
#include "state_machine/EasyStateMachine.h"
#include "state_machine/StateMachine.h"


#include <stdio.h>
#include <execinfo.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

void handler(int sig) {
    void *array[30];
    size_t size;

    // get void*'s for all entries on the stack
    size = backtrace(array, 30);

    // print out all the frames to stderr
    fprintf(stderr, "Error: signal %d:\n", sig);
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    exit(1);
}

int main(int argc, char **argv) {
    signal(SIGSEGV, handler);   // install our handler
    signal(SIGINT, handler);   // install our handler
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
        throw exception;
    }
    return 0;
}