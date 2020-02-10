cmake_minimum_required(VERSION 3.14)
project(raft)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS " -DBOOST_LOG_DYN_LINK")


add_compile_options(-fno-elide-constructors)

set(BOOST_ROOT /Users/ynx/boost/build)
include(FindProtobuf)
find_package(protobuf REQUIRED)

if (protobuf_VERBOSE)
    message(STATUS "Using Protocol Buffers ${Protobuf_VERSION}")
endif ()


find_package(Boost COMPONENTS log log_setup serialization REQUIED)
include_directories(${Boost_INCLUDE_DIRS})


add_executable(raft
        main.cpp
        log/boost_lop.cpp
        rpc/rpc.cpp
        rpc.proto
        entry/entry.cpp
        util.cpp
        entry/data.cpp
        Timer.cpp
        state_machine/StateMachineControler.cpp
        state_machine/EasyStateMachine.cpp
        RaftServer.cpp
        rpc/rpc_to_string.h
        rpc/rpc_to_string.cpp
        rpc/rpc_type.h
        rpc/SocketMap.cpp
        rpc/SocketMap.h)
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


add_executable(client client/main.cpp
        client.cpp
        log/boost_lop.cpp
        rpc/rpc.cpp
        rpc.proto
        util.cpp
        state_machine/EasyStateMachine.cpp)
#add_executable(test_thread_pool state_machine/test_thread_pool.cpp)
#add_executable(state_machine_controler state_machine/StateMachineControler.cpp)


#add_executable(test_read_file tests/test_read_file.cpp)

#add_executable(test_map_insert tests/test_map_insert.cpp)


#add_executable(boost_log_trivial tests/boost_log_trivial.cpp)
#
#target_link_libraries(boost_log_trivial
#        ${Boost_LOG_LIBRARY}
#        #        ${Boost_LOG_SETUP_LIBRARY}
#        ${Boost_THREAD_LIBRARY}
#
#        )

#        ${Boost_FILE_SYSTEM_LIBRARY}
#        ${Boost_SYSTEM_LIBRARY})

#add_executable(test_boost_timer_bug tests/boost_timer_bug.cpp
#        )
#target_link_libraries(test_boost_timer_bug
#        ${Boost_LOG_LIBRARY}
#        ${Boost_LOG_SETUP_LIBRARY}
#        ${Boost_SERIALIZATION_LIBRARY}
#        ${Boost_WSERIALIZATION_LIBRARY}
#        ${Boost_FILE_SYSTEM_LIBRARY}
#        ${Boost_THREAD_LIBRARY}
#        ${Boost_SYSTEM_LIBRARY})

#add_executable(boost_test_timer tests/boost_test_timer.cpp)
#target_link_libraries(boost_test_timer
#        ${Boost_LOG_LIBRARY}
#        ${Boost_LOG_SETUP_LIBRARY}
#        ${Boost_SERIALIZATION_LIBRARY}
#        ${Boost_WSERIALIZATION_LIBRARY}
#        ${Boost_FILE_SYSTEM_LIBRARY}
#        ${Boost_THREAD_LIBRARY}
#        ${Boost_SYSTEM_LIBRARY})

#add_executable(boost_timer_bug2 tests/boost_timer_bug2.cpp)
#target_link_libraries(boost_timer_bug2
#        ${Boost_LOG_LIBRARY}
#        ${Boost_LOG_SETUP_LIBRARY}
#        ${Boost_SERIALIZATION_LIBRARY}
#        ${Boost_WSERIALIZATION_LIBRARY}
#        ${Boost_FILE_SYSTEM_LIBRARY}
#        ${Boost_THREAD_LIBRARY}
#        ${Boost_SYSTEM_LIBRARY})

#if (protobuf_MODULE_COMPATIBLE) #Legacy Support
#    protobuf_generate_cpp(main_PROTO_SRCS main_PROTO_HDRS rpc.proto)
#    list(APPEND main_SRCS ${main_PROTO_SRCS} ${main_PROTO_HDRS})
#endif ()


#add_executable(test_boost_dispatch_vs_post tests/boost_dispatch_vs_post.cpp)
#target_link_libraries(test_boost_dispatch_vs_post
#        ${Boost_LOG_LIBRARY}
#        ${Boost_LOG_SETUP_LIBRARY}
#        ${Boost_SERIALIZATION_LIBRARY}
#        ${Boost_WSERIALIZATION_LIBRARY}
#        ${Boost_FILE_SYSTEM_LIBRARY}
#        ${Boost_THREAD_LIBRARY}
#        ${Boost_SYSTEM_LIBRARY})

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


#add_executable(boost_timer_test tests/boost_timer_hook.cpp)


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
