#include <iostream>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>

//#include <boost/log/core.hpp>
namespace logging = boost::log;
namespace keywords = boost::log::keywords;
namespace attrs = boost::log::attributes;

void init_logging();
