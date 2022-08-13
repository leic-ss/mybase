include_directories(BEFORE ${PROJECT_SOURCE_DIR})

# kv client api
set(KV_CLIENT_API_SRC ${CLIENT_SRC_PATH}/kv_client_api.cc)

add_library(kv_client_api_lib STATIC ${KV_CLIENT_API_SRC})
set_target_properties(kv_client_api_lib PROPERTIES COMPILE_FLAGS "-fPIC")
add_library(KV_CLIENT_API_OBJ OBJECT ${KV_CLIENT_API_SRC})

# kv client
set(KV_CLIENT_SRC ${CLIENT_SRC_PATH}/kv_client.cc)
add_executable(kvclient ${KV_CLIENT_SRC}
                   	    $<TARGET_OBJECTS:KV_CLIENT_API_OBJ>
                        $<TARGET_OBJECTS:COMMON_OBJ>
                        $<TARGET_OBJECTS:PUBLIC_OBJ>
                        $<TARGET_OBJECTS:PROTO_OBJ>)
target_link_libraries(kvclient ${PROTOBUF_STATIC_LIBRARIES} ${LIBPTHREAD} ${LIBZ} ${LIBRT}
                              ${DEPS_PREFIX_PATH}/lib/libevent.a
                              ${DEPS_PREFIX_PATH}/lib/libevent_pthreads.a)
