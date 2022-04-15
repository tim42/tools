

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/ntools_tests)

if (LIBURING_FOUND)
  add_executable(io_test io.cpp)
else()
  add_executable(io_test async.cpp) # dummy
endif()

add_executable(async_test async.cpp)
add_executable(threading_test threading.cpp)


foreach(target io_test async_test threading_test)
  target_compile_options(${target} PRIVATE ${NTOOLS_FLAGS})
  target_include_directories(${target} PRIVATE SYSTEM ${LIBURING_INCLUDE_DIR})
  target_link_libraries(${target} PUBLIC ntools ${HUGETLBFS_LIBRARIES})
endforeach()
