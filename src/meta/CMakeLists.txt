include_directories(BEFORE ${PROJECT_SOURCE_DIR})

link_directories(${DEPS_PREFIX_PATH}/lib)
# link_directories(${DEPS_PREFIX_PATH}/lib64)

# AUX_SOURCE_DIRECTORY(${METASERVER_SRC_PATH} METASERVER_SRC)

set(METASERVER_SRC ${METASERVER_SRC_PATH}/meta.cc
                   ${METASERVER_SRC_PATH}/server.cc
                   ${METASERVER_SRC_PATH}/table_mgr.cc
                   ${METASERVER_SRC_PATH}/stat_info.cc
                   ${METASERVER_SRC_PATH}/table_builder.cc
                   ${METASERVER_SRC_PATH}/sys_mgr.cc
                   ${METASERVER_SRC_PATH}/kv_state_machine.cc)

add_executable(meta ${METASERVER_SRC}
                          $<TARGET_OBJECTS:COMMON_OBJ>
                          $<TARGET_OBJECTS:PUBLIC_OBJ>
                          $<TARGET_OBJECTS:PROTO_OBJ>)

target_link_libraries(meta ${FULL_BUILD_DEPS})
