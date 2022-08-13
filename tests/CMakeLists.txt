include_directories(BEFORE ${PROJECT_SOURCE_DIR})

link_directories(${DEPS_PREFIX_PATH}/lib)
link_directories(${DEPS_PREFIX_PATH}/lib64)

set(LIBGTEST gtest)

set(FULL_GTEST_BUILD_DEPS
    ${FULL_BUILD_DEPS}
    ${LIBGTEST}
)

message(STATUS "TESTS CMAKE_SOURCE_DIR : " ${CMAKE_SOURCE_DIR})
message(STATUS "TESTS PROJECT_SOURCE_DIR : " ${PROJECT_SOURCE_DIR})
message(STATUS "TEST PROJECT_BINARY_DIR : " ${PROJECT_BINARY_DIR})

AUX_SOURCE_DIRECTORY(${PROJECT_TEST_SRC_PATH}/ut UNIT_TEST_SRC)
add_executable(ut ${UNIT_TEST_SRC}
                  $<TARGET_OBJECTS:COMMON_OBJ>
                  $<TARGET_OBJECTS:PUBLIC_OBJ>
                  $<TARGET_OBJECTS:PACKETS_OBJ>
                  $<TARGET_OBJECTS:PROTOS_OBJ>)
target_link_libraries(ut ${FULL_GTEST_BUILD_DEPS})

AUX_SOURCE_DIRECTORY(${PROJECT_TEST_SRC_PATH}/st SYS_TEST_SRC)
add_executable(st ${SYS_TEST_SRC}
                  $<TARGET_OBJECTS:COMMON_OBJ>
                  $<TARGET_OBJECTS:PUBLIC_OBJ>
                  $<TARGET_OBJECTS:PACKETS_OBJ>)
target_link_libraries(st ${FULL_GTEST_BUILD_DEPS})
