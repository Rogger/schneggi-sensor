add_executable(battery_tests test_battery.c ../../../src/app_battery.c
	../../../src/app_measurement_logic.c)
add_executable(environment_tests test_environment.c ../../../src/app_environment.c
	../../../src/app_measurement_logic.c ../../../src/app_zcl_report.c)
foreach(target battery_tests environment_tests)
	target_include_directories(${target} PRIVATE stubs ../shtc3/stubs ../zcl_report/stubs
		../../../include ../../../src)
	target_compile_features(${target} PRIVATE c_std_99)
	add_test(NAME ${target} COMMAND ${target})
endforeach()
