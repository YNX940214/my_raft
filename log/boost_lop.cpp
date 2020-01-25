#include "boost_lop.h"

void init_logging() {
    logging::register_simple_formatter_factory<logging::trivial::severity_level, char>("Severity");

    logging::add_file_log(
            keywords::file_name = "sample.log",
            keywords::format = "[%TimeStamp%] [%ThreadID%] [%Severity%] [%LineID%] %Message%"
    );

    logging::core::get()->set_filter
            (
                    logging::trivial::severity >= logging::trivial::trace
            );


    logging::add_common_attributes();
}
