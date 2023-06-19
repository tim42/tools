##
## CMAKE file for neam/...
##

cmake_minimum_required(VERSION 3.21)

project(ntools VERSION 0.2.0)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# We need liburing for async IO
# (without it, no io is compiled)
find_package(LibUring)

set(PUBLIC_LIBS )

set(io_srcs)
if (LIBURING_FOUND)
  message(STATUS "Found liburing (${LIBURING_LIBRARY}), building with neam::io")
  set(io_srcs io/context.cpp)
  set(PUBLIC_LIBS ${PUBLIC_LIBS} ${LIBURING_LIBRARY})
endif()

# create the library
add_library(ntools
    logger/logger.cpp
    id/string_id.cpp
    threading/task.cpp
    threading/task_group_graph.cpp
    threading/task_manager.cpp
    rle/serialization_metadata.cpp
    rle/generic_type.cpp
    struct_metadata/type_to_string.cpp

    memory.cpp
    sys_utils.cpp

    ${io_srcs}
    test.cpp

    demangle.hpp
)

# Workaround kdevelop refusing the C++23 standard if set in cmake...

# this is the proper way, but kdevelop does not like it
#set_property(TARGET ntools PROPERTY CXX_STANDARD 23)

if (${USE_TRACY})
  message(STATUS "ntools: Profiling with Tracy enabled")
  add_compile_definitions(TRACY_ENABLE=1)
endif()

if (${USE_TRACY})
  if (TARGET TracyClient)
    set(PUBLIC_LIBS ${PUBLIC_LIBS} TracyClient)
  endif()
endif()


if(MSVC)
  set(NTOOLS_FLAGS /W4 /WX /std:c++latest)
else()
  # It has to be 2b or kdevelop refuses to parse .cpp files
  set(NTOOLS_FLAGS -Wall -Wextra -std=gnu++2b -Wno-invalid-offsetof  -Wno-unused-parameter)
endif()

target_compile_options(ntools PRIVATE ${NTOOLS_FLAGS})

if (TARGET fmt)
    set(PUBLIC_LIBS ${PUBLIC_LIBS} fmt)
endif()

target_link_libraries(ntools PUBLIC ${PUBLIC_LIBS})


target_include_directories(ntools PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  set(NTOOLS_MASTER_PROJECT ON)
else()
  set(NTOOLS_MASTER_PROJECT OFF)
endif ()

option(BUILD_NTOOLS_TESTS "Build ntools test executables" ${NTOOLS_MASTER_PROJECT})

if (${BUILD_NTOOLS_TESTS})
  message(STATUS "ntools: building test executables")
  add_subdirectory(_tests/)
endif()
