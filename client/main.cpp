#include "client.h"
#include "../state_machine/EasyStateMachine.h"
#include <iostream>
#include "../log/boost_lop.h"

using namespace std;

int main(int argc, char **argv) {
    try {
        init_logging(9999);
        int port = atoi(argv[1]);
        io_service ioContext;
        std::tuple<string, int> server = std::make_tuple("127.0.0.1", port);
        StateMachine *esm = new EasyStateMachine();
        client client_(ioContext, server, esm);
        client_.run();
    } catch (std::exception &exception) {
        cout << "exception: " << exception.what();
    }

}