include_directories(BEFORE ${PROJECT_SOURCE_DIR})

# link_directories(${DEPS_PREFIX_PATH}/lib)
# link_directories(${DEPS_PREFIX_PATH}/lib64)

message(STATUS "DEPS_PREFIX_PATH: " ${DEPS_PREFIX_PATH})
message(STATUS "PROJECT_SOURCE_DIR: " ${PROJECT_SOURCE_DIR})

set(BENCHMARKKV_SRC_PATH ${TOOLS_SRC_PATH}/benchmarkkv)
set(BENCHMARKKV_SRC ${BENCHMARKKV_SRC_PATH}/main.cc
                  ${BENCHMARKKV_SRC_PATH}/benchmark.cc
                  $<TARGET_OBJECTS:COMMON_OBJ>
                  $<TARGET_OBJECTS:PUBLIC_OBJ>
                  $<TARGET_OBJECTS:PACKETS_OBJ>
                  $<TARGET_OBJECTS:KV_CLIENT_API_OBJ2>
                  $<TARGET_OBJECTS:PROTO_OBJ>)

add_executable(benchmarkkv ${BENCHMARKKV_SRC})
target_link_libraries(benchmarkkv ${FULL_BUILD_DEPS})