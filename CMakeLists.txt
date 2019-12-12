cmake_minimum_required(VERSION 3.14)
project(raft)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS " -DBOOST_LOG_DYN_LINK")

set(BOOST_ROOT /Users/ynx/boost/build)
#include_directories(/Users/ynx/boost/build/include)
#link_directories(/Users/ynx/boost/build/lib)

find_package(Boost COMPONENTS log log_setup REQUIED)
include_directories(${Boost_INCLUDE_DIRS})
#add_executable(raft main.cpp log/boost_lop.cpp rpc/rpc.cpp)
target_link_libraries(raft pthread ${Boost_LOG_LIBRARY} ${Boost_LOG_SETUP_LIBRARY} ${Boost_FILE_SYSTEM_LIBRARY} ${Boost_THREAD_LIBRARY} ${Boost_SYSTEM_LIBRARY})

