#include "boost_lop.h"

void init_logging() {
    logging::register_simple_formatter_factory<logging::trivial::severity_level, char>("Severity");

    logging::add_file_log(
            keywords::file_name = "sample.log",
            keywords::format = "[%TimeStamp%] [%ThreadID%] [%Severity%] [%ProcessID%] [%LineID%] %Message%"
    );

    logging::core::get()->set_filter
            (
                    logging::trivial::severity >= logging::trivial::debug
            );


    logging::add_common_attributes();
}
