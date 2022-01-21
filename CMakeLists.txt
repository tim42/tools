##
## CMAKE file for neam/...
##

cmake_minimum_required(VERSION 3.11)

project(ntools VERSION 0.2.0)


add_library(ntools
    logger/logger.cpp
    io/context.cpp
    test.cpp
)

# Workaround kdevelop refusing the C++23 standard if set in cmake...

# this is the proper way, but kdevelop does not like it
#set_property(TARGET ntools PROPERTY CXX_STANDARD 23)


if(MSVC)
  target_compile_options(ntools PRIVATE /W4 /WX /std:c++latest)
else()
  # It has to be 2b or kdevelop refuses to parse .cpp files
  target_compile_options(ntools PRIVATE -Wall -Wextra -std=gnu++2b)
endif()

if (TARGET fmt)
    target_link_libraries(ntools fmt)
endif()


target_include_directories(ntools PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
