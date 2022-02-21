
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}")

# We need liburing for async IO
find_package(LibUring REQUIRED)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/ntools_tests)

add_executable(io_test io.cpp)
add_executable(async_test async.cpp)
add_executable(threading_test threading.cpp)


foreach(target io_test async_test threading_test)
  target_compile_options(${target} PRIVATE ${NTOOLS_FLAGS})
  target_include_directories(${target} PRIVATE SYSTEM ${LIBURING_INCLUDE_DIR})
  target_link_libraries(${target} PUBLIC ${LIBURING_LIBRARY})
  target_link_libraries(${target} PUBLIC ntools)
  if (TARGET fmt)
    target_link_libraries(${target} PUBLIC fmt)
  endif()
  if (${USE_TRACY})
    if (TARGET TracyClient)
      target_link_libraries(${target} PUBLIC TracyClient)
    endif()
  endif()
endforeach()
