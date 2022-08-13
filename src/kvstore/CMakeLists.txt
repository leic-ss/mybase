include_directories(BEFORE ${PROJECT_SOURCE_DIR})

# link_directories(${DEPS_PREFIX_PATH}/lib)
# link_directories(${DEPS_PREFIX_PATH}/lib64)

set(KVSTORE_SRC ${KVSTORE_SRC_PATH}/rdb/rdb_define.cc
            	${KVSTORE_SRC_PATH}/rdb/rdb_manager.cc
           		${KVSTORE_SRC_PATH}/rdb/rdb_instance.cc
            	$<TARGET_OBJECTS:PUBLIC_OBJ>)

add_library(kvstore_lib STATIC ${KVSTORE_SRC})
set_target_properties(kvstore_lib PROPERTIES COMPILE_FLAGS "-fPIC")

add_library(KVSTORE_OBJ OBJECT ${KVSTORE_SRC})