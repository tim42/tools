##
## CMAKE file for neam/...
##

cmake_minimum_required(VERSION 2.8)


set(PROJ_VERSION_MAJOR 0)
set(PROJ_VERSION_MINOR 1)
set(PROJ_VERSION_SUPERMINOR 1)

if (${CMAKE_PROJECT_NAME} STREQUAL ${PROJECT_NAME})
  add_definitions(-D_PROJ_VERSION_MAJOR=${PROJ_VERSION_MAJOR})
  add_definitions(-D_PROJ_VERSION_MINOR=${PROJ_VERSION_MINOR})
  add_definitions(-D_PROJ_VERSION_SUPERMINOR=${PROJ_VERSION_SUPERMINOR})
endif()

set(PROJ_VERSION_NAME "")

set(PROJ_NAME ntools)

if (PROJ_VERSION_NAME STREQUAL "")
  set(PROJ_APP ${PROJ_NAME}-${PROJ_VERSION_MAJOR}.${PROJ_VERSION_MINOR}.${PROJ_VERSION_SUPERMINOR})
else ()
  set(PROJ_APP ${PROJ_NAME}-${PROJ_VERSION_NAME}-${PROJ_VERSION_MAJOR}.${PROJ_VERSION_MINOR}.${PROJ_VERSION_SUPERMINOR})
endif()

project(${PROJ_APP})

message(STATUS "-----------------------------------------------")
message(STATUS "Building neam/${PROJ_NAME} :: ${PROJ_APP}")
message(STATUS "-----------------------------------------------")


if (CMAKE_BUILD_TYPE STREQUAL "")
  # CMake defaults to leaving CMAKE_BUILD_TYPE empty. This screws up
  # differentiation between debug and release builds.
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build, options are: None (CMAKE_CXX_FLAGS or CMAKE_C_FLAGS used) Debug Release MinSizeRel." FORCE)
endif ()

# output
set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/")
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_CURRENT_BINARY_DIR}/")
set(LIBRARY_OUTPUT_PATH "${CMAKE_CURRENT_BINARY_DIR}/libs")

# flags
# general flags
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(PROJ_FLAGS "${PROJ_FLAGS} -O0 -g3")
else(CMAKE_BUILD_TYPE STREQUAL "Release")
  set(PROJ_FLAGS "${PROJ_FLAGS} -DNDEBUG")
else(CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
  set(PROJ_FLAGS "${PROJ_FLAGS} -DNDEBUG -fmerge-all-constants")
endif()

# some gcc/clang flags
if (${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
  set(PROJ_FLAGS "${PROJ_FLAGS} -std=gnu++11 -Wall -Wextra -Wno-unused-function")

  if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(PROJ_FLAGS "${PROJ_FLAGS} -finline-limit=0")
  else(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(PROJ_FLAGS "${PROJ_FLAGS} -O3")
  else(CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
    set(PROJ_FLAGS "${PROJ_FLAGS} -Os")
  endif()

elseif (${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
  set(PROJ_FLAGS "${PROJ_FLAGS} -std=c++11 -Wall -Wextra -Wno-unused-function")

  if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(PROJ_FLAGS "${PROJ_FLAGS}")
  else(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(PROJ_FLAGS "${PROJ_FLAGS} -O3")
  else(CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
    set(PROJ_FLAGS "${PROJ_FLAGS} -Oz")
  endif()
endif()

# build a static lib
if (NOT TARGET ${PROJ_APP})
  add_definitions(${PROJ_FLAGS})

  set(PROJ_SOURCES  logger/logger.cpp
                    
  )
  add_library(${PROJ_APP} STATIC ${PROJ_SOURCES})

  # install
  install(TARGETS ${PROJ_APP} DESTINATION lib/neam)
  install(DIRECTORY ./ DESTINATION include/neam/tools
          FILES_MATCHING PATTERN "*.hpp"
          PATTERN ".git" EXCLUDE
          PATTERN "cmake" EXCLUDE
          PATTERN "samples" EXCLUDE)
endif()

# export project name to parent project
if (NOT ${CMAKE_PROJECT_NAME} STREQUAL ${PROJECT_NAME})
  set(lib${PROJ_NAME} ${PROJ_APP} PARENT_SCOPE)
endif()

