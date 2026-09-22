add_executable(test_ota test_ota.c)
target_include_directories(test_ota PRIVATE stubs ../../../include)
add_test(NAME ota COMMAND test_ota)
