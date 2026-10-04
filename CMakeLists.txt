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
if(CMAKE_MATCH_1 GREATER 255 OR CMAKE_MATCH_2 GREATER 255 OR CMAKE_MATCH_3 GREATER 65535)
  message(FATAL_ERROR "OTA version exceeds the Zigbee version field widths")
endif()

project("Schneggi Sensor")

target_sources(app PRIVATE
	src/app_environment.c
	src/app_shtc3.c
	src/app_network.c
	src/app_ota.c
	src/app_reboot.c
	src/app_scheduler.c
	src/app_measurement_logic.c
	src/app_zcl_report.c
	src/co2_zcl_logic.c
	src/main.c
	src/rejoin_logic.c
	src/zigbee_signal_logic.c
	src/zcl/zb_zcl_concentration_measurement.c
)

target_sources_ifdef(CONFIG_ADC app PRIVATE src/app_battery.c)

target_include_directories(app PRIVATE include)
# NORDIC SDK APP END
