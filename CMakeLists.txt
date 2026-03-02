enable_testing()

add_executable(core_test tests/core_test.c)
target_include_directories(core_test PRIVATE ${CMAKE_SOURCE_DIR}/core/primitives)
add_test(NAME CoreTest COMMAND core_test)
