

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/ntools_tests)

if (LIBURING_FOUND)
  add_executable(io_test io.cpp)
  add_executable(io_server_test io_server.cpp)
else()
  add_executable(io_test async.cpp) # dummy
  add_executable(io_server_test async.cpp) # dummy
endif()

add_executable(async_test async.cpp)
add_executable(threading_test threading.cpp)

add_executable(rpc_target_a rpc_target_a.cpp rpc_stubs.cpp)
add_executable(rpc_target_b rpc_target_b.cpp rpc_stubs.cpp)


foreach(target io_test io_server_test async_test threading_test rpc_target_a rpc_target_b)
  target_compile_options(${target} PRIVATE ${NTOOLS_FLAGS})
  target_include_directories(${target} PRIVATE SYSTEM ${LIBURING_INCLUDE_DIR})
  target_link_libraries(${target} PUBLIC ntools ${HUGETLBFS_LIBRARIES})
endforeach()
