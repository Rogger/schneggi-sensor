add_executable(scheduler_tests test_scheduler.c ../../../src/app_scheduler.c)
add_executable(network_tests test_network.c ../../../src/app_scheduler.c
	../../../src/app_reboot.c ../../../src/rejoin_logic.c ../../../src/zigbee_signal_logic.c)
foreach(target scheduler_tests network_tests)
	target_include_directories(${target} PRIVATE stubs ../ota/stubs ../../../include)
	target_compile_features(${target} PRIVATE c_std_99)
	add_test(NAME ${target} COMMAND ${target})
endforeach()
