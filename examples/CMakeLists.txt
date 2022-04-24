cmake_minimum_required(VERSION 3.1)

project(
  co_lib_examples
  VERSION 1.0
  LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set( CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -stdlib=libc++")

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

add_executable(sender_receiver sender_receiver.cpp)
target_link_libraries(sender_receiver ${CONAN_LIBS})

add_executable(introduction introduction.cpp)
target_link_libraries(introduction ${CONAN_LIBS})

add_executable(tcp_echo_client tcp_echo_client.cpp)
target_link_libraries(tcp_echo_client ${CONAN_LIBS})

add_executable(tcp_echo_server tcp_echo_server.cpp)
target_link_libraries(tcp_echo_server ${CONAN_LIBS})

add_executable(worker_pool worker_pool.cpp)
target_link_libraries(worker_pool ${CONAN_LIBS})
