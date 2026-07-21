cmake_minimum_required(VERSION 3.16)

project(measurement_logic_tests C)

enable_testing()

add_executable(measurement_logic_tests
	test_measurement_logic.c
	../../../src/app_measurement_logic.c
)

target_include_directories(measurement_logic_tests PRIVATE
	../../../include
)

target_compile_features(measurement_logic_tests PRIVATE c_std_99)

add_test(NAME measurement_logic_tests COMMAND measurement_logic_tests)
