#include "boost_lop.h"

void init_logging(int port) {
    logging::register_simple_formatter_factory<logging::trivial::severity_level, char>("Severity");

    logging::add_file_log(
            keywords::file_name = "raft_" + std::to_string(port) + ".log",
            keywords::format = "[%TimeStamp%] [%ThreadID%] [%Severity%] [%LineID%] %Message%",
            keywords::auto_flush = true
    );

    logging::core::get()->set_filter
            (
                    logging::trivial::severity >= logging::trivial::trace
            );


    logging::add_common_attributes();
}
