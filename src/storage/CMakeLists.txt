include_directories(BEFORE ${PROJECT_SOURCE_DIR})

link_directories(${DEPS_PREFIX_PATH}/lib)
# link_directories(${DEPS_PREFIX_PATH}/lib64)

AUX_SOURCE_DIRECTORY(${STORAGESERVER_SRC_PATH} STORAGESERVER_SRC)

add_executable(storage ${STORAGESERVER_SRC}
                       $<TARGET_OBJECTS:COMMON_OBJ>
                       $<TARGET_OBJECTS:PUBLIC_OBJ>
                       $<TARGET_OBJECTS:PROTO_OBJ>
                       $<TARGET_OBJECTS:KVSTORE_OBJ>
                       $<TARGET_OBJECTS:KVSTORE_OBJ>)

target_link_libraries(storage ${FULL_BUILD_DEPS})