#include "client.h"
#include "../state_machine/EasyStateMachine.h"

int main() {
    io_service ioContext;
    std::tuple<string, int> server = std::make_tuple<string, int>("127.0.0.1", 6667);
    StateMachine *esm = new EasyStateMachine();
    client client_(ioContext, server, esm);
    client_.run();
}