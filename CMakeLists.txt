#
# Copyright (c) 2020 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project("Schneggi Sensor")

target_sources(app PRIVATE
	src/app_measurement_logic.c
	src/app_zcl_report.c
	src/co2_zcl_logic.c
	src/main.c
	src/rejoin_logic.c
	src/zigbee_signal_logic.c
	src/zcl/zb_zcl_concentration_measurement.c
)

target_include_directories(app PRIVATE include)
# NORDIC SDK APP END
