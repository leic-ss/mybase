include_directories(BEFORE ${PROJECT_SOURCE_DIR})

AUX_SOURCE_DIRECTORY(${PUBLIC_SRC_PATH} PUBLIC_SRC)

add_library(public_lib STATIC ${PUBLIC_SRC})
set_target_properties(public_lib PROPERTIES COMPILE_FLAGS "-fPIC")

add_library(PUBLIC_OBJ OBJECT ${PUBLIC_SRC})
