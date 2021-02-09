
file(GLOB sources CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_executable(co_lib_test ${sources})

target_link_libraries(co_lib_test co_lib CONAN_PKG::catch2)

include(CTest)
include(Catch)
catch_discover_tests(co_lib_test)