cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(co_lib LANGUAGES CXX)

# this will help to use conan's libcxx profile
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")

include(cmake/conan.cmake)
conan_cmake_run(CONANFILE conanfile.py
        BASIC_SETUP CMAKE_TARGETS NO_OUTPUT_DIRS
        BUILD missing)

include(cmake/CPM.cmake)

add_library(co_lib
        include/co/event.cpp
        include/co/impl/timer.cpp
        include/co/impl/scheduler.cpp
        include/co/impl/waiting_queue.cpp
        include/co/thread.cpp
        include/co/this_thread.cpp
        include/co/mutex.cpp)

target_link_libraries(co_lib PUBLIC ${CONAN_LIBS})
target_link_directories(co_lib PUBLIC ${CONAN_LIB_DIRS})
target_include_directories(co_lib PUBLIC ${CMAKE_SOURCE_DIR}/include ${CONAN_INCLUDE_DIRS})

enable_testing()
add_subdirectory(tests)
