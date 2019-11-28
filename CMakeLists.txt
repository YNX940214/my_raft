cmake_minimum_required(VERSION 3.14)
project(raft)

set(CMAKE_CXX_STANDARD 14)

set(BOOST_ROOT /Users/ynx/boost/build)
#include_directories(/Users/ynx/boost/build/include)
#link_directories(/Users/ynx/boost/build/lib)

find_package(Boost REQUIRED COMPONENTS )
include_directories( ${Boost_INCLUDE_DIRS} )
add_executable(raft main.cpp)
target_link_libraries( raft pthread )
