include_directories(BEFORE ${PROJECT_SOURCE_DIR})

AUX_SOURCE_DIRECTORY(${PACKETS_SRC_PATH} PACKETS_SRC)

add_library(packets_lib STATIC ${PACKETS_SRC})
set_target_properties(packets_lib PROPERTIES COMPILE_FLAGS "-fPIC")

add_library(PACKETS_OBJ OBJECT ${PACKETS_SRC})
