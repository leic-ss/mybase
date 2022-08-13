include_directories(BEFORE ${PROJECT_SOURCE_DIR})

AUX_SOURCE_DIRECTORY(${RAFT_SRC_PATH} RAFT_SRC)

add_library(raft_lib STATIC ${RAFT_SRC})
set_target_properties(raft_lib PROPERTIES COMPILE_FLAGS "-fPIC")

add_library(RAFT_OBJ OBJECT ${RAFT_SRC})
