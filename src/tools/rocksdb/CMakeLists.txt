include_directories(BEFORE ${PROJECT_SOURCE_DIR})

# link_directories(${DEPS_PREFIX_PATH}/lib)
# link_directories(${DEPS_PREFIX_PATH}/lib64)

set(FULL_ROCKSDB_BUILD_DEPS
    ${FULL_BUILD_DEPS}
    ${DEPS_PREFIX_PATH}/lib64/librocksdb.a
)

message(STATUS "DEPS_PREFIX_PATH: " ${DEPS_PREFIX_PATH})
message(STATUS "PROJECT_SOURCE_DIR: " ${PROJECT_SOURCE_DIR})

set(TOOLS_ROCKSDB_SRC_PATH ${TOOLS_SRC_PATH}/rocksdb)
set(TOOLS_ROCKSDB_SRC_TEST ${TOOLS_ROCKSDB_SRC_PATH}/db_test.cc
		                   $<TARGET_OBJECTS:COMMON_OBJ>
		                   $<TARGET_OBJECTS:PUBLIC_OBJ>
		                   $<TARGET_OBJECTS:PACKETS_OBJ>)

add_executable(rocksdb_test ${TOOLS_ROCKSDB_SRC_TEST})
target_link_libraries(rocksdb_test ${FULL_ROCKSDB_BUILD_DEPS})


set(TOOLS_SSTFILEWRITER_SRC_TEST ${TOOLS_ROCKSDB_SRC_PATH}/sst_file_writer.cc
			                   	 $<TARGET_OBJECTS:COMMON_OBJ>
			                   	 $<TARGET_OBJECTS:PUBLIC_OBJ>
			                   	 $<TARGET_OBJECTS:PACKETS_OBJ>)

add_executable(sst_file_writer_test ${TOOLS_SSTFILEWRITER_SRC_TEST})
target_link_libraries(sst_file_writer_test ${FULL_ROCKSDB_BUILD_DEPS})

set(TOOLS_SSTFILEDELETE_SRC_TEST ${TOOLS_ROCKSDB_SRC_PATH}/sst_file_delete.cc
			                   	 $<TARGET_OBJECTS:COMMON_OBJ>
			                   	 $<TARGET_OBJECTS:PUBLIC_OBJ>
			                   	 $<TARGET_OBJECTS:PACKETS_OBJ>)

add_executable(sst_file_delete ${TOOLS_SSTFILEDELETE_SRC_TEST})
target_link_libraries(sst_file_delete ${FULL_ROCKSDB_BUILD_DEPS})
