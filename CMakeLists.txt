##
## CMAKE file for neam/...
##

cmake_minimum_required(VERSION 3.11)

project(ntools VERSION 0.2.0)


add_library(ntools
    logger/logger.cpp
    io/context.cpp
    threading/task.cpp
    threading/task_group_graph.cpp
    threading/task_manager.cpp
    test.cpp
)

# Workaround kdevelop refusing the C++23 standard if set in cmake...

# this is the proper way, but kdevelop does not like it
#set_property(TARGET ntools PROPERTY CXX_STANDARD 23)

if (${USE_TRACY})
  message(STATUS "ntools: Profiling with Tracy enabled")
  add_compile_definitions(TRACY_ENABLE=1)
endif()

if(MSVC)
  set(NTOOLS_FLAGS /W4 /WX /std:c++latest)
else()
  # It has to be 2b or kdevelop refuses to parse .cpp files
  set(NTOOLS_FLAGS -Wall -Wextra -std=gnu++2b)
endif()

target_compile_options(ntools PRIVATE ${NTOOLS_FLAGS})

if (TARGET fmt)
    target_link_libraries(ntools fmt)
endif()


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
