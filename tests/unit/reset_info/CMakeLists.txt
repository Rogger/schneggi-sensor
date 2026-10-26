add_executable(reset_info_tests test_reset_info.c ../../../src/app_reset_info.c)
target_include_directories(reset_info_tests PRIVATE stubs ../../../include)
target_compile_features(reset_info_tests PRIVATE c_std_99)
add_test(NAME reset_info_tests COMMAND reset_info_tests)

add_executable(reset_report_tests test_reset_report.c ../../../src/app_reset_report.c
	../../../src/app_reset_info.c)
target_include_directories(reset_report_tests PRIVATE report_stubs stubs ../../../include)
target_compile_features(reset_report_tests PRIVATE c_std_99)
add_test(NAME reset_report_tests COMMAND reset_report_tests)
