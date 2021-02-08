add_executable(libuv_hello_world libuv_hello_world.cpp)
target_link_libraries(libuv_hello_world ${CONAN_LIBS})

# add_executable(redis_client redis_client.cpp)
# target_link_libraries(redis_client ${CONAN_LIBS})

add_executable(redis_connection redis_connection.cpp)
target_link_libraries(redis_connection ${CONAN_LIBS})

add_executable(redis_bench redis_bench.cpp)
target_link_libraries(redis_bench ${CONAN_LIBS})