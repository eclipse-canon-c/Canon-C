enable_testing()

add_executable(core_test tests/core_test.c)
add_test(NAME CoreTest COMMAND core_test)
