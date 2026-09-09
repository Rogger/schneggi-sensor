cmake_minimum_required(VERSION 3.16)

project(schneggi_unit_tests C)

enable_testing()

# These tests use assert(), including in Release configurations.
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Werror -UNDEBUG)
elseif(MSVC)
  add_compile_options(/UNDEBUG)
endif()

add_subdirectory(zigbee_signal_logic)
add_subdirectory(co2_zcl)
add_subdirectory(rejoin_logic)
add_subdirectory(measurement_logic)
add_subdirectory(zcl_report)
add_subdirectory(shtc3)
