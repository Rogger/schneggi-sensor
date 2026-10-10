add_executable(battery_tests test_battery.c ../../../src/app_battery.c
	../../../src/app_measurement_logic.c)
add_executable(environment_tests test_environment.c ../../../src/app_environment.c
	../../../src/app_measurement_logic.c ../../../src/app_zcl_report.c)
add_executable(scd4x_tests test_scd4x.c ../../../src/app_scd4x.c
	../../../src/app_environment.c ../../../src/app_measurement_logic.c
	../../../src/app_zcl_report.c ../../../src/co2_zcl_logic.c)
foreach(target battery_tests environment_tests scd4x_tests)
	target_include_directories(${target} PRIVATE stubs ../shtc3/stubs ../zcl_report/stubs
		../../../include ../../../src)
	target_compile_features(${target} PRIVATE c_std_99)
	add_test(NAME ${target} COMMAND ${target})
endforeach()
