#
# Copyright (c) 2020 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

if(NOT CONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)$")
  message(FATAL_ERROR "OTA version must be major.minor.patch without a build suffix")
endif()
if(CMAKE_MATCH_1 GREATER 255 OR CMAKE_MATCH_2 GREATER 255 OR CMAKE_MATCH_3 GREATER 255)
  message(FATAL_ERROR "OTA version components must be 0..255 for the Zigbee add-on generator")
endif()
math(EXPR SCHNEGGI_OTA_FILE_VERSION
  "(${CMAKE_MATCH_1} << 24) | (${CMAKE_MATCH_2} << 16) | ${CMAKE_MATCH_3}"
  OUTPUT_FORMAT HEXADECIMAL)

project("Schneggi Sensor")
target_compile_definitions(app PRIVATE SCHNEGGI_OTA_FILE_VERSION=${SCHNEGGI_OTA_FILE_VERSION})

target_sources(app PRIVATE
	src/app_environment.c
	src/app_shtc3.c
	src/app_network.c
	src/app_ota.c
	src/app_ota_storage.c
	src/app_ota_transfer.c
	src/app_reboot.c
	src/app_scheduler.c
	src/app_measurement_logic.c
	src/app_zcl_report.c
	src/co2_zcl_logic.c
	src/main.c
	src/rejoin_logic.c
	src/zigbee_signal_logic.c
	src/zboss_mem_globals.c
	src/zcl/zb_zcl_concentration_measurement.c
)

target_sources_ifdef(CONFIG_ADC app PRIVATE src/app_battery.c)
target_sources_ifdef(CONFIG_SCD4X app PRIVATE src/app_scd4x.c)

target_include_directories(app PRIVATE include)

# Nordic's prebuilt library has plain ELF weak defaults for these configurable
# globals. Resolve Schneggi's strong medium-sized tables outside LTO as well.
if(CONFIG_LTO)
  set_source_files_properties(src/zboss_mem_globals.c PROPERTIES COMPILE_OPTIONS -fno-lto)
endif()
# NORDIC SDK APP END
