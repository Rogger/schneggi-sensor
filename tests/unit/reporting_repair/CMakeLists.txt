add_executable(reporting_repair_tests test_reporting_repair.c ../../../src/app_reporting_repair.c)
target_include_directories(reporting_repair_tests PRIVATE stubs ../../../include)
target_compile_features(reporting_repair_tests PRIVATE c_std_99)
add_test(NAME reporting_repair_tests COMMAND reporting_repair_tests)
