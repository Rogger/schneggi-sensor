cmake_minimum_required(VERSION 3.16)

project(zcl_report_tests C)

enable_testing()

add_executable(zcl_report_tests
	test_zcl_report.c
	../../../src/app_zcl_report.c
)

target_include_directories(zcl_report_tests PRIVATE
	stubs
	../../../include
	../../../src
)

target_compile_features(zcl_report_tests PRIVATE c_std_99)

add_test(NAME zcl_report_tests COMMAND zcl_report_tests)
