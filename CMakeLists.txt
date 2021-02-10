cmake_minimum_required(VERSION 3.1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(co_lib LANGUAGES CXX)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(NO_OUTPUT_DIRS TARGETS)

set(CMAKE_CXX_FLAGS_DEBUG
    "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address,undefined"
)
set(CMAKE_LINKER_FLAGS_DEBUG
    "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address,undefined"
)

include(cmake/CPM.cmake)

add_library(co_lib INTERFACE IMPORTED)
target_link_libraries(co_lib INTERFACE ${CONAN_LIBS})
target_link_directories(co_lib INTERFACE ${CONAN_LIB_DIRS})
target_include_directories(co_lib INTERFACE ${CMAKE_SOURCE_DIR}/include ${CONAN_INCLUDE_DIRS})

enable_testing()
add_subdirectory(test)
