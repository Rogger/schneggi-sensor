cmake_minimum_required(VERSION 3.16)

project(rejoin_logic_tests C)

enable_testing()

add_executable(rejoin_logic_tests
	test_rejoin_logic.c
	../../../src/rejoin_logic.c
)

target_include_directories(rejoin_logic_tests PRIVATE
	../../../include
)

target_compile_features(rejoin_logic_tests PRIVATE c_std_99)

add_test(NAME rejoin_logic_tests COMMAND rejoin_logic_tests)
