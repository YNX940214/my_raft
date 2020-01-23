#include <iostream>
#include <vector>
#include <string>
#include "util.h"
#include <iostream>
#include "../log/boost_lop.h"

using namespace std;

int main() {
    init_logging();
    BOOST_LOG_TRIVIAL(trace) << "This is a trace severity message";
    BOOST_LOG_TRIVIAL(debug) << "This is a debug severity message";
    BOOST_LOG_TRIVIAL(info) << "This is an informational severity message";
    BOOST_LOG_TRIVIAL(warning) << "This is a warning severity message";
    BOOST_LOG_TRIVIAL(error) << "This is an error severity message";
    BOOST_LOG_TRIVIAL(fatal) << "and this is a fatal severity message";
    return 0;
}