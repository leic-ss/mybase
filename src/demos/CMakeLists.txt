include_directories(BEFORE ${PROJECT_SOURCE_DIR})

# link_directories(${DEPS_PREFIX_PATH}/lib)
# link_directories(${DEPS_PREFIX_PATH}/lib64)

set(DEMO_SAMPLEF_SRC_PATH ${PROJECT_SRC}/demos/sample)
set(DEMO_RPC_SRC_PATH ${PROJECT_SRC}/demos/rpc)
set(DEMO_OTHERS_SRC_PATH ${PROJECT_SRC}/demos/others)

#set(DEMO_KVCLIENT_API_SRC ${DEMO_SAMPLEF_SRC_PATH}/kvclientapi.cc)
#add_executable(kvclientapi ${DEMO_KVCLIENT_API_SRC}
#                           $<TARGET_OBJECTS:COMMON_OBJ>
#                           $<TARGET_OBJECTS:PUBLIC_OBJ>
#                           $<TARGET_OBJECTS:PACKETS_OBJ>
#                           $<TARGET_OBJECTS:KV_CLIENT_API_OBJ2>)
#target_link_libraries(kvclientapi ${FULL_BUILD_DEPS})

set(DEMO_RPC_CLIENT_SRC ${DEMO_RPC_SRC_PATH}/client.cc)
add_executable(rpclient ${DEMO_RPC_CLIENT_SRC}
                           $<TARGET_OBJECTS:COMMON_OBJ>
                           $<TARGET_OBJECTS:PUBLIC_OBJ>)
target_link_libraries(rpclient ${FULL_BUILD_DEPS})

set(DEMO_RPC_SERVER_SRC ${DEMO_RPC_SRC_PATH}/server.cc)
add_executable(rpcserver ${DEMO_RPC_SERVER_SRC}
                           $<TARGET_OBJECTS:COMMON_OBJ>
                           $<TARGET_OBJECTS:PUBLIC_OBJ>)
target_link_libraries(rpcserver ${FULL_BUILD_DEPS})

# set(DEMO_PDBCLIENT_SRC ${DEMO_SAMPLEF_SRC_PATH}/pdbclientdemo.cc)
# add_executable(pdbclientdemo ${DEMO_PDBCLIENT_SRC}
#                         $<TARGET_OBJECTS:COMMON_OBJ>
#                         $<TARGET_OBJECTS:PUBLIC_OBJ>
#                         $<TARGET_OBJECTS:PACKETS_OBJ>
#                         $<TARGET_OBJECTS:KV_CLIENT_API_OBJ2>
#                         $<TARGET_OBJECTS:PDB_CLIENT_API_OBJ>
#                         $<TARGET_OBJECTS:PROTO_OBJ>)
#target_link_libraries(pdbclientdemo ${FULL_BUILD_DEPS})

#set(DEMO_PDBCLIENT_SRC ${DEMO_SAMPLEF_SRC_PATH}/protoupdate.cc)
#add_executable(protoupdate ${DEMO_PDBCLIENT_SRC}
#                           $<TARGET_OBJECTS:COMMON_OBJ>
#                           $<TARGET_OBJECTS:PUBLIC_OBJ>
#                           $<TARGET_OBJECTS:PACKETS_OBJ>
#                           $<TARGET_OBJECTS:KV_CLIENT_API_OBJ2>
#                           $<TARGET_OBJECTS:PDB_CLIENT_API_OBJ>
#                           $<TARGET_OBJECTS:PROTO_OBJ>)
#target_link_libraries(protoupdate ${FULL_BUILD_DEPS})

#set(DEMO_ASYNCKVCLIENT_API_SRC ${DEMO_SAMPLEF_SRC_PATH}/asynckvclientapi.cc)
#add_executable(asynckvclientapi ${DEMO_ASYNCKVCLIENT_API_SRC}
#                                $<TARGET_OBJECTS:COMMON_OBJ>
#                                $<TARGET_OBJECTS:PUBLIC_OBJ>
#                                $<TARGET_OBJECTS:PACKETS_OBJ>
#                                $<TARGET_OBJECTS:KV_CLIENT_API_OBJ2>)
#target_link_libraries(asynckvclientapi ${FULL_BUILD_DEPS})

#set(DEMO_FIFOQUEUE_SRC ${DEMO_SAMPLEF_SRC_PATH}/fifoqueue.cc
#                       $<TARGET_OBJECTS:COMMON_OBJ>
#                       $<TARGET_OBJECTS:PUBLIC_OBJ>
#                       $<TARGET_OBJECTS:PACKETS_OBJ>)

#add_executable(fifoqueue ${DEMO_FIFOQUEUE_SRC})
#target_link_libraries(fifoqueue ${FULL_BUILD_DEPS})

#set(LOGGERWRITER_SRC ${DEMO_SAMPLEF_SRC_PATH}/loggerwriter.cc
#                           $<TARGET_OBJECTS:COMMON_OBJ>
#                           $<TARGET_OBJECTS:PUBLIC_OBJ>
#                           $<TARGET_OBJECTS:PACKETS_OBJ>)

#add_executable(logwriter ${LOGGERWRITER_SRC})
#target_link_libraries(logwriter ${FULL_BUILD_DEPS})


#set(DEMO_THREADPOOL_SRC ${DEMO_SAMPLEF_SRC_PATH}/threadpool.cc
#                         $<TARGET_OBJECTS:COMMON_OBJ>
#                         $<TARGET_OBJECTS:PUBLIC_OBJ>
#                         $<TARGET_OBJECTS:PACKETS_OBJ>)

#add_executable(threadpool ${DEMO_THREADPOOL_SRC})
#target_link_libraries(threadpool ${FULL_BUILD_DEPS})

#set(DEMO_QUEUETHREAD_SRC ${DEMO_SAMPLEF_SRC_PATH}/queuethread.cc
#                         $<TARGET_OBJECTS:COMMON_OBJ>
#                         $<TARGET_OBJECTS:PUBLIC_OBJ>
#                         $<TARGET_OBJECTS:PACKETS_OBJ>)

#add_executable(queuethread ${DEMO_QUEUETHREAD_SRC})
#target_link_libraries(queuethread ${FULL_BUILD_DEPS})

#set(DEMO_SIGDEMO_SRC ${DEMO_OTHERS_SRC_PATH}/sigdemo.cc
#                    $<TARGET_OBJECTS:COMMON_OBJ>
#                    $<TARGET_OBJECTS:PUBLIC_OBJ>
#                    $<TARGET_OBJECTS:PACKETS_OBJ>)
#add_executable(sigdemo ${DEMO_SIGDEMO_SRC})
#target_link_libraries(sigdemo ${FULL_BUILD_DEPS})

#set(DEMO_EVTIMER_SRC ${DEMO_SAMPLEF_SRC_PATH}/evtimer.cc
#                    $<TARGET_OBJECTS:COMMON_OBJ>
#                    $<TARGET_OBJECTS:PUBLIC_OBJ>
#                    $<TARGET_OBJECTS:PACKETS_OBJ>)
#add_executable(evtimer ${DEMO_EVTIMER_SRC})
#target_link_libraries(evtimer ${FULL_BUILD_DEPS})