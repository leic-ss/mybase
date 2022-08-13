include_directories(BEFORE ${PROJECT_SOURCE_DIR})

# link_directories(${DEPS_PREFIX_PATH}/lib)
# link_directories(${DEPS_PREFIX_PATH}/lib64)

message(STATUS "DEPS_PREFIX_PATH: " ${DEPS_PREFIX_PATH})
message(STATUS "PROJECT_SOURCE_DIR: " ${PROJECT_SOURCE_DIR})

set(IMAGE_SRC_PATH ${TOOLS_SRC_PATH}/image)
set(IMAGE_LOAD_SRC ${IMAGE_SRC_PATH}/imageload.cc
		           $<TARGET_OBJECTS:COMMON_OBJ>
		           $<TARGET_OBJECTS:COMMON_KV_OBJ>
		           $<TARGET_OBJECTS:PUBLIC_OBJ>
		           $<TARGET_OBJECTS:PACKETS_OBJ>)

add_executable(imageload ${IMAGE_LOAD_SRC})
target_link_libraries(imageload ${FULL_BUILD_DEPS})

set(IMAGE_REPLAY_SRC ${IMAGE_SRC_PATH}/imagereplay.cc
		           $<TARGET_OBJECTS:COMMON_OBJ>
		           $<TARGET_OBJECTS:COMMON_KV_OBJ>
		           $<TARGET_OBJECTS:PUBLIC_OBJ>
		           $<TARGET_OBJECTS:PACKETS_OBJ>
		           $<TARGET_OBJECTS:KV_CLIENT_API_OBJ2>)

add_executable(imagereplay ${IMAGE_REPLAY_SRC})
target_link_libraries(imagereplay ${FULL_BUILD_DEPS})

set(IMAGE_REPLAY2_SRC ${IMAGE_SRC_PATH}/imagereplay2.cc
		           $<TARGET_OBJECTS:COMMON_OBJ>
		           $<TARGET_OBJECTS:COMMON_KV_OBJ>
		           $<TARGET_OBJECTS:PUBLIC_OBJ>
		           $<TARGET_OBJECTS:PACKETS_OBJ>
		           $<TARGET_OBJECTS:KV_CLIENT_API_OBJ2>)

add_executable(imagereplay2 ${IMAGE_REPLAY2_SRC})
target_link_libraries(imagereplay2 ${FULL_BUILD_DEPS})

set(BINLOG_READER_SRC ${IMAGE_SRC_PATH}/logreader.cc
			           $<TARGET_OBJECTS:COMMON_OBJ>
			           $<TARGET_OBJECTS:PUBLIC_OBJ>
			           $<TARGET_OBJECTS:PACKETS_OBJ>)

add_executable(logreader ${BINLOG_READER_SRC})
target_link_libraries(logreader ${FULL_BUILD_DEPS})
