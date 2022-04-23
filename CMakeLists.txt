cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(co_lib LANGUAGES CXX)

# this will help to use conan's libcxx profile
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
endif()

if (WIN32)
    add_definitions(-DNOMINMAX)
endif()

include(cmake/conan.cmake)
conan_cmake_run(CONANFILE conanfile.py
        BASIC_SETUP CMAKE_TARGETS NO_OUTPUT_DIRS
        BUILD missing)

include(cmake/CPM.cmake)

add_library(co_lib
        src/co/impl/timer.cpp
        src/co/impl/async_signal.cpp
        src/co/impl/scheduler.cpp
        src/co/impl/thread_storage.cpp
        src/co/thread.cpp
        src/co/this_thread.cpp
        src/co/mutex.cpp
        src/co/net/tcp_listener.cpp
        src/co/net/impl/uv_tcp_ptr.cpp
        src/co/net/tcp_stream.cpp
        src/co/condition_variable.cpp
        src/co/signal_callback.cpp
        src/co/exception.cpp
        src/co/net/address.cpp)

target_link_libraries(co_lib PUBLIC ${CONAN_LIBS})
target_link_directories(co_lib PUBLIC ${CONAN_LIB_DIRS})
target_include_directories(co_lib PUBLIC ${CMAKE_SOURCE_DIR}/src ${CONAN_INCLUDE_DIRS})

enable_testing()
add_subdirectory(tests)
