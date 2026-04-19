# Minimal Makefile for Zephyr/NCS app builds

# Optional local overrides (not committed)
-include local.mk

# --- Configurable paths ---
TOOLCHAIN_PATH ?= $(shell find $$HOME/ncs/toolchains -mindepth 1 -maxdepth 1 -type d 2>/dev/null | sort | tail -n1)
TOOLCHAIN_PYTHON ?= $(firstword $(wildcard $(TOOLCHAIN_PATH)/usr/local/bin/python3.*) $(wildcard $(TOOLCHAIN_PATH)/usr/local/bin/python3))
XDG_CACHE_HOME ?= /tmp/codex-cache

# --- Build config ---
BOARD ?= adafruit_feather_nrf52840
CONF_FILE ?= prj_debug_no_scd4x.conf
OVERLAY ?= boards/adafruit_feather_nrf52840.overlay;boards/no_scd4x.overlay
BUILD_DIR ?= build_default
APP_DIR := $(CURDIR)
NCS_WORKSPACE ?= $(if $(and $(wildcard $(APP_DIR)/../.west),$(wildcard $(APP_DIR)/../zephyr)),$(abspath $(APP_DIR)/..),$(shell find $$HOME/ncs -mindepth 1 -maxdepth 1 -type d -name 'v*' 2>/dev/null | sort | tail -n1))
BUILD_DIR_ABS = $(if $(filter /%,$(BUILD_DIR)),$(BUILD_DIR),$(APP_DIR)/$(BUILD_DIR))
HEX ?= $(BUILD_DIR_ABS)/merged.hex

# --- Flash config ---
SNR ?= 1050218947
NRFJPROG ?= nrfjprog
WEST ?= west

WEST_ENV = \
	PATH="$(TOOLCHAIN_PATH)/bin:$(TOOLCHAIN_PATH)/usr/bin:$(TOOLCHAIN_PATH)/usr/local/bin:$(TOOLCHAIN_PATH)/opt/bin:$$PATH" \
	LD_LIBRARY_PATH="$(TOOLCHAIN_PATH)/lib:$(TOOLCHAIN_PATH)/lib/x86_64-linux-gnu:$(TOOLCHAIN_PATH)/usr/local/lib:$${LD_LIBRARY_PATH:-}" \
	PYTHONHOME="$(TOOLCHAIN_PATH)/usr/local" \
	XDG_CACHE_HOME="$(XDG_CACHE_HOME)" \
	ZEPHYR_TOOLCHAIN_VARIANT=zephyr \
	ZEPHYR_SDK_INSTALL_DIR="$(TOOLCHAIN_PATH)/opt/zephyr-sdk" \
	CCACHE_DISABLE=1

.PHONY: help check check-toolchain west-update build build-no-scd4x build-scd4x build-debug build-debug-co2 build-production build-production-co2 test clean erase flash erase-and-flash flash-debug flash-debug-co2 flash-production flash-production-co2

help:
	@echo "Targets: west-update build build-no-scd4x build-scd4x build-debug build-debug-co2 build-production build-production-co2 test clean erase flash erase-and-flash flash-debug flash-debug-co2 flash-production flash-production-co2"
	@echo "Overrides: TOOLCHAIN_PATH TOOLCHAIN_PYTHON XDG_CACHE_HOME NCS_WORKSPACE BOARD CONF_FILE OVERLAY BUILD_DIR SNR"

check:
	@test -x "$(TOOLCHAIN_PYTHON)" || (echo "Missing TOOLCHAIN_PYTHON: $(TOOLCHAIN_PYTHON)"; exit 1)
	@mkdir -p "$(XDG_CACHE_HOME)"
	@test -d "$(NCS_WORKSPACE)/.west" || (echo "Invalid NCS_WORKSPACE: $(NCS_WORKSPACE)"; exit 1)
	@test -d "$(NCS_WORKSPACE)/zephyr" || (echo "Missing zephyr dir in NCS_WORKSPACE"; exit 1)
	@if ! printf '%s' "$(OVERLAY)" | grep -q 'boards/no_scd4x.overlay'; then \
		test -d "$(NCS_WORKSPACE)/modules/sensirion_drivers" || (echo "Missing SCD4X module in workspace. Run: make west-update"; exit 1); \
	fi

check-toolchain: check

west-update: check-toolchain
	@cd "$(NCS_WORKSPACE)" && env $(WEST_ENV) $(WEST) update

build: check
	@cd "$(NCS_WORKSPACE)" && env $(WEST_ENV) $(WEST) build \
		-s "$(APP_DIR)" -d "$(BUILD_DIR_ABS)" -b "$(BOARD)" -- \
		-DNCS_TOOLCHAIN_VERSION=NONE \
		-DWEST_PYTHON="$(TOOLCHAIN_PYTHON)" \
		-DCONF_FILE="$(CONF_FILE)" \
		-DDTC_OVERLAY_FILE="$(OVERLAY)"

build-no-scd4x: CONF_FILE=prj_debug_no_scd4x.conf
build-no-scd4x: BUILD_DIR=build_no_scd4x
build-no-scd4x: OVERLAY=boards/adafruit_feather_nrf52840.overlay;boards/no_scd4x.overlay
build-no-scd4x: build

build-scd4x: CONF_FILE=prj_debug.conf
build-scd4x: BUILD_DIR=build_scd4x
build-scd4x: OVERLAY=boards/adafruit_feather_nrf52840.overlay
build-scd4x: build

build-debug: BUILD_DIR=build_debug
build-debug: CONF_FILE=prj_debug_no_scd4x.conf
build-debug: OVERLAY=boards/adafruit_feather_nrf52840.overlay;boards/no_scd4x.overlay
build-debug: build

build-debug-co2: BUILD_DIR=build_debug_co2
build-debug-co2: CONF_FILE=prj_debug.conf
build-debug-co2: OVERLAY=boards/adafruit_feather_nrf52840.overlay
build-debug-co2: build

build-production: BUILD_DIR=build_production
build-production: CONF_FILE=prj_production_no_scd4x.conf
build-production: OVERLAY=boards/adafruit_feather_nrf52840.overlay;boards/no_scd4x.overlay
build-production: build

build-production-co2: BUILD_DIR=build_production_co2
build-production-co2: CONF_FILE=prj_production.conf
build-production-co2: OVERLAY=boards/adafruit_feather_nrf52840.overlay
build-production-co2: build

test:
	@cmake -S tests/unit -B tests/unit/build
	@cmake --build tests/unit/build
	@ctest --test-dir tests/unit/build --output-on-failure

clean:
	@if [ -d "$(BUILD_DIR_ABS)" ]; then \
		rm -rf "$(BUILD_DIR_ABS)"; \
		echo "Removed $(BUILD_DIR_ABS)"; \
	else \
		echo "$(BUILD_DIR_ABS) does not exist, nothing to clean."; \
	fi

erase:
	@$(NRFJPROG) -f NRF52 --snr $(SNR) --eraseall

flash:
	@$(NRFJPROG) -f NRF52 --snr $(SNR) --sectorerase --program "$(HEX)" --verify --reset

erase-and-flash: erase flash

flash-debug: BUILD_DIR=build_debug
flash-debug: flash

flash-debug-co2: BUILD_DIR=build_debug_co2
flash-debug-co2: flash

flash-production: BUILD_DIR=build_production
flash-production: flash

flash-production-co2: BUILD_DIR=build_production_co2
flash-production-co2: flash
