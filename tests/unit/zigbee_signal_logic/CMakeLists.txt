cmake_minimum_required(VERSION 3.16)

project(zigbee_signal_logic_tests C)

enable_testing()

add_executable(zigbee_signal_logic_tests
	test_signal_logic.c
	../../../src/zigbee_signal_logic.c
)

target_include_directories(zigbee_signal_logic_tests PRIVATE
	../../../include
)

target_compile_features(zigbee_signal_logic_tests PRIVATE c_std_99)

add_test(NAME zigbee_signal_logic_tests COMMAND zigbee_signal_logic_tests)
