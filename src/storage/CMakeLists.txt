include_directories(BEFORE ${PROJECT_SOURCE_DIR})

link_directories(${DEPS_PREFIX_PATH}/lib)
# link_directories(${DEPS_PREFIX_PATH}/lib64)

# AUX_SOURCE_DIRECTORY(${STORAGESERVER_SRC_PATH} STORAGESERVER_SRC)

set(STORAGESERVER_SRC ${STORAGESERVER_SRC_PATH}/storage.cc
                      ${STORAGESERVER_SRC_PATH}/server.cc
                      ${STORAGESERVER_SRC_PATH}/heartbeat.cc
                      ${STORAGESERVER_SRC_PATH}/processor.cc
                      ${STORAGESERVER_SRC_PATH}/bucket_mgr.cc
                      ${STORAGESERVER_SRC_PATH}/peer.cc
                      ${STORAGESERVER_SRC_PATH}/migrate_mgr.cc)

add_executable(storage ${STORAGESERVER_SRC}
                       $<TARGET_OBJECTS:COMMON_OBJ>
                       $<TARGET_OBJECTS:PUBLIC_OBJ>
                       $<TARGET_OBJECTS:PROTO_OBJ>
                       $<TARGET_OBJECTS:KVSTORE_OBJ>)

target_link_libraries(storage ${FULL_BUILD_DEPS})