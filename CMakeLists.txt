cmake_minimum_required(VERSION 3.5)
project(mybase CXX)

SET(CMAKE_VERBOSE_MAKEFILE on)

message(STATUS "CMAKE_SOURCE_DIR : " ${CMAKE_SOURCE_DIR})
message(STATUS "PROJECT_SOURCE_DIR : " ${PROJECT_SOURCE_DIR})
message(STATUS "PROJECT_BINARY_DIR : " ${PROJECT_BINARY_DIR})
message(STATUS "CMAKE_BINARY_DIR : " ${CMAKE_BINARY_DIR})
message(STATUS "CMAKE_INSTALL_PREFIX : " ${CMAKE_INSTALL_PREFIX})

EXECUTE_PROCESS(COMMAND git describe --abbrev=0 --always OUTPUT_STRIP_TRAILING_WHITESPACE OUTPUT_VARIABLE GIT_VERSION)
message(STATUS "GIT_VERSION: " ${GIT_VERSION})

set(PROJECT_VERSION_BUILD ${GIT_VERSION})
message(STATUS "PROJECT_VERSION_BUILD: " ${PROJECT_VERSION_BUILD})

if (DEPS_PREFIX_PATH)
    message(STATUS "DEPS_PREFIX_PATH: " ${DEPS_PREFIX_PATH})
else()
    set(DEPS_PREFIX_PATH ${PROJECT_SOURCE_DIR}/deps_prefix)
    message(STATUS "DEPS_PREFIX_PATH is not given, Default: " ${DEPS_PREFIX_PATH})
endif()

set(PROJECT_SRC ${PROJECT_SOURCE_DIR}/src)

set(CMAKE_DIR ${PROJECT_SOURCE_DIR}/cmake)
set(CMAKE_MODULE_PATH ${CMAKE_DIR})
set(protobuf_MODULE_COMPATIBLE TRUE)

# Protobuf
include(${CMAKE_DIR}/protobuf.cmake)

include_directories(${PROTOBUF_INCLUDE_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})
message(STATUS "PROTOBUF_INCLUDE_DIR : " ${PROTOBUF_INCLUDE_DIR})
message(STATUS "CMAKE_CURRENT_BINARY_DIR : " ${CMAKE_CURRENT_BINARY_DIR})

include_directories(${DEPS_PREFIX_PATH}/include)

set(PROTO_SRC_PATH ${PROJECT_SRC}/proto)
set(TOOLS_SRC_PATH ${ROOT_SRC}/tools)
set(PROJECT_TEST_SRC_PATH ${PROJECT_SOURCE_DIR}/tests)
set(COMMON_SRC_PATH ${PROJECT_SRC}/common)
set(PUBLIC_SRC_PATH ${PROJECT_SRC}/public)
set(DEMOS_SRC_PATH ${PROJECT_SRC}/demos)
set(METASERVER_SRC_PATH ${PROJECT_SRC}/meta)
set(STORAGESERVER_SRC_PATH ${PROJECT_SRC}/storage)
set(KVSTORE_SRC_PATH ${PROJECT_SRC}/kvstore)
set(CLIENT_SRC_PATH ${PROJECT_SRC}/client)

include_directories(${PROJECT_SRC})

option(COVERAGE "Enable Coverage" OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug" CACHE STRING
      "Choose the type of build, options are: Debug Release."
      FORCE)
endif()

message(STATUS "CMAKE_BUILD_TYPE : " ${CMAKE_BUILD_TYPE})
if (CMAKE_BUILD_TYPE STREQUAL Debug)
   set(CMAKE_CXX_FLAGS "-O0")
else()
   set(CMAKE_CXX_FLAGS "-O2")
endif()

set(LIBPTHREAD pthread)
set(LIBRT rt)
set(LIBLZMA lzma)
set(LIBDL dl)
set(LIBZ z)

set(FULL_BUILD_DEPS
    ${PROTOBUF_STATIC_LIBRARIES}
    ${LIBPTHREAD}
    ${LIBRT}
    ${LIBDL}
    ${LIBZ}
    ${DEPS_PREFIX_PATH}/lib/libevent.a
    ${DEPS_PREFIX_PATH}/lib/libevent_pthreads.a
    ${DEPS_PREFIX_PATH}/lib/libraftcc.a
    ${DEPS_PREFIX_PATH}/lib64/librocksdb.a
)

if(CMAKE_BUILD_LZMA)
    list(APPEND FULL_BUILD_DEPS ${LIBLZMA})
    message(STATUS "CMAKE_BUILD_LZMA : Enabled")
elseif(NOT CMAKE_BUILD_LZMA)
    message(STATUS "CMAKE_BUILD_LZMA : Disabled")
endif()

option(CMAKE_USE_OLD_GDB "cmake to support old gdb" OFF)
if(CMAKE_USE_OLD_GDB)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -gdwarf-3")
    message(STATUS "CMAKE_USE_OLD_GDB : Enabled")
elseif(NOT CMAKE_USE_OLD_GDB)
    message(STATUS "CMAKE_USE_OLD_GDB : Disabled")
endif()

file(GLOB PROTO_SRC_FILES ${PROTO_SRC_PATH}/*.proto)
# file(GLOB_RECURSE PROTO_SRC_FILES ${PROTO_SRC_PATH}/*.proto)
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_SRC_FILES})
message(STATUS "${PROTO_SRCS} ${PROTO_HDRS} ${PROTO_SRC_FILES}")

set(PROTO_SRCS ${PROTO_SRCS} ${PROTO_HDRS})
add_library(PROTO_OBJ OBJECT ${PROTO_SRCS})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

set(CMAKE_C_FLAGS " ${CMAKE_C_FLAGS} -DPROJECT_VERSION=\\\"${PROJECT_VERSION_BUILD}\\\"")
set(CMAKE_CXX_FLAGS " ${CMAKE_CXX_FLAGS} -DPROJECT_VERSION=\\\"${PROJECT_VERSION_BUILD}\\\"")
set(CMAKE_CXX_FLAGS " ${CMAKE_CXX_FLAGS} -DGIT_VERSION=\\\"${GIT_VERSION}\\\" ")

add_definitions(-D__STDC_FORMAT_MACROS)
add_definitions(-D_LARGEFILE_SOURCE)

set(LD_FLAGS "${LD_FLAGS} -static-libgcc -static-libstdc++")

add_subdirectory(${PROJECT_SRC}/public public)
add_subdirectory(${PROJECT_SRC}/common common)
add_subdirectory(${PROJECT_SRC}/demos demos)
add_subdirectory(${PROJECT_SRC}/meta meta)
add_subdirectory(${PROJECT_SRC}/storage storage)
add_subdirectory(${PROJECT_SRC}/kvstore kvstore)
add_subdirectory(${PROJECT_SRC}/client client)
# add_subdirectory(${PROJECT_SOURCE_DIR}/tests tests)
