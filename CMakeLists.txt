cmake_minimum_required(VERSION 3.10)
project(Canon-C)
enable_testing()

add_executable(bits_test test/core/primitives/bits_test.c)
target_include_directories(bits_test PRIVATE ${CMAKE_SOURCE_DIR}/core/primitives)
add_test(NAME BitsTest COMMAND bits_test)

add_executable(checked_test test/core/primitives/checked_test.c)
target_include_directories(checked_test PRIVATE ${CMAKE_SOURCE_DIR}/core/primitives)
add_test(NAME CheckedTest COMMAND checked_test)

add_executable(compare_test test/core/primitives/compare_test.c)
target_include_directories(compare_test PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/core/primitives)
find_library(LIBM m)
if(LIBM)
    target_link_libraries(compare_test PRIVATE ${LIBM})
endif()
add_test(NAME CompareTest COMMAND compare_test)
