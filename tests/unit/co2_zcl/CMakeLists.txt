cmake_minimum_required(VERSION 3.16)

project(co2_zcl_logic_tests C)

enable_testing()

add_executable(co2_zcl_logic_tests
	test_co2_zcl_logic.c
	../../../src/co2_zcl_logic.c
)

target_include_directories(co2_zcl_logic_tests PRIVATE
	../../../include
)

target_compile_features(co2_zcl_logic_tests PRIVATE c_std_99)

add_test(NAME co2_zcl_logic_tests COMMAND co2_zcl_logic_tests)
