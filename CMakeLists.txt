cmake_minimum_required(VERSION 3.10)
project(Canon-C)

enable_testing()

add_executable(bits_test test/core/primitives/bits_test.c)
target_include_directories(core_test PRIVATE ${CMAKE_SOURCE_DIR}/core/primitives)

add_test(NAME BitsTest COMMAND bits_test)
