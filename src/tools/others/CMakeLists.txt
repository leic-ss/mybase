include_directories(BEFORE ${PROJECT_SOURCE_DIR})

# link_directories(${DEPS_PREFIX_PATH}/lib)
# link_directories(${DEPS_PREFIX_PATH}/lib64)

message(STATUS "DEPS_PREFIX_PATH: " ${DEPS_PREFIX_PATH})
message(STATUS "PROJECT_SOURCE_DIR: " ${PROJECT_SOURCE_DIR})

set(TOOLS_OTHERS_SRC_PATH ${TOOLS_SRC_PATH}/others)
set(COMPARE_SRC ${TOOLS_SRC_PATH}/others/compare.cc)
add_executable(compare ${COMPARE_SRC})

set(STAT_SRC ${TOOLS_SRC_PATH}/others/stat.cc)
add_executable(stat ${STAT_SRC})