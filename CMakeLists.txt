cmake_minimum_required(VERSION 3.14)
project(raft)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS " -DBOOST_LOG_DYN_LINK")


set(BOOST_ROOT /Users/ynx/boost/build)
include(FindProtobuf)
find_package(protobuf REQUIRED)

if (protobuf_VERBOSE)
    message(STATUS "Using Protocol Buffers ${Protobuf_VERSION}")
endif ()


find_package(Boost COMPONENTS log log_setup serialization REQUIED)
include_directories(${Boost_INCLUDE_DIRS})


if (protobuf_MODULE_COMPATIBLE) #Legacy Support
    protobuf_generate_cpp(main_PROTO_SRCS main_PROTO_HDRS rpc.proto)
    list(APPEND main_SRCS ${main_PROTO_SRCS} ${main_PROTO_HDRS})
endif ()


add_executable(raft main.cpp
        log/boost_lop.cpp
        rpc/rpc.cpp
        rpc.proto
        entry/entry.cpp
        entry/entry.h
#        StateMachine.cpp
#        StateMachine.h
        const.h
        entry/data.cpp
        entry/data.h
        )
if (protobuf_MODULE_COMPATIBLE) #Legacy mode
    target_include_directories(raft PUBLIC ${PROTOBUF_INCLUDE_DIRS})
    target_link_libraries(raft ${PROTOBUF_LIBRARIES})
else ()
    target_link_libraries(raft
            protobuf::libprotobuf
            ${Boost_LOG_LIBRARY}
            ${Boost_LOG_SETUP_LIBRARY}
            ${Boost_SERIALIZATION_LIBRARY}
            ${Boost_WSERIALIZATION_LIBRARY}
            ${Boost_FILE_SYSTEM_LIBRARY}
            ${Boost_THREAD_LIBRARY}
            ${Boost_SYSTEM_LIBRARY})
    protobuf_generate(TARGET raft)
endif ()


#add_executable(test_entry
#        rpc.proto
#        entry/entry.cpp
#        entry/entry.h
#        entry/data.cpp
#        entry/data.h)
#target_link_libraries(test_entry
#        protobuf::libprotobuf
#        ${Boost_LOG_LIBRARY}
#        ${Boost_LOG_SETUP_LIBRARY}
#        ${Boost_SERIALIZATION_LIBRARY}
#        ${Boost_WSERIALIZATION_LIBRARY}
#        ${Boost_FILE_SYSTEM_LIBRARY}
#        ${Boost_THREAD_LIBRARY}
#        ${Boost_SYSTEM_LIBRARY}
#        )
#protobuf_generate(TARGET test_entry)


#add_executable(test_boost_archive tests/boost_archive.cpp)
#message(${Boost_SERIALIZATION_LIBRARY})
#target_link_libraries(test_boost_archive ${Boost_SERIALIZATION_LIBRARY})

#add_executable(test test.cpp)
#add_executable(test_entry entry/entry.cpp)

#add_executable(test_log tests/boost_log.cpp log/boost_lop.cpp)
#target_link_libraries(test_log
#        ${Boost_LOG_LIBRARY}
#        ${Boost_LOG_SETUP_LIBRARY}
#        ${Boost_THREAD_LIBRARY}
#        )
