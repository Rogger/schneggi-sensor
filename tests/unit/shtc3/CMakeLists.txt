add_executable(shtc3_tests test_shtc3.c ../../../src/app_shtc3.c)
target_include_directories(shtc3_tests PRIVATE stubs ../../../include)
add_test(NAME shtc3_tests COMMAND shtc3_tests)
