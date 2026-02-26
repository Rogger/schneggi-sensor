cmake_minimum_required(VERSION 3.16)

project(schneggi_unit_tests C)

enable_testing()

add_subdirectory(zigbee_signal_logic)
add_subdirectory(co2_zcl)
