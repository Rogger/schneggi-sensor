# Minimal Makefile for Zephyr/NCS app builds

# Optional local overrides (not committed)
-include local.mk

# --- Configurable paths ---
TOOLCHAIN_PATH ?= $(shell find $$HOME/ncs/toolchains -mindepth 1 -maxdepth 1 -type d 2>/dev/null | sort | tail -n1)
TOOLCHAIN_PYTHON ?= $(firstword $(wildcard $(TOOLCHAIN_PATH)/usr/local/bin/python3.*) $(wildcard $(TOOLCHAIN_PATH)/usr/local/bin/python3))

# --- Build config ---
BOARD ?= adafruit_feather_nrf52840
CONF_FILE ?= prj_debug.conf
OVERLAY ?= boards/adafruit_feather_nrf52840.overlay;boards/no_scd4x.overlay
BUILD_DIR ?= build_west
APP_DIR := $(CURDIR)
NCS_WORKSPACE ?= $(if $(wildcard $(APP_DIR)/../.west),$(abspath $(APP_DIR)/..),$(shell find $$HOME/ncs -mindepth 1 -maxdepth 1 -type d -name 'v*' 2>/dev/null | sort | tail -n1))
BUILD_DIR_ABS := $(if $(filter /%,$(BUILD_DIR)),$(BUILD_DIR),$(APP_DIR)/$(BUILD_DIR))
HEX ?= $(BUILD_DIR_ABS)/merged.hex

# --- Flash config ---
SNR ?= 1050218947
NRFJPROG ?= nrfjprog
WEST ?= west

WEST_ENV = \
	PATH="$(TOOLCHAIN_PATH)/bin:$(TOOLCHAIN_PATH)/usr/bin:$(TOOLCHAIN_PATH)/usr/local/bin:$(TOOLCHAIN_PATH)/opt/bin:$$PATH" \
	LD_LIBRARY_PATH="$(TOOLCHAIN_PATH)/lib:$(TOOLCHAIN_PATH)/lib/x86_64-linux-gnu:$(TOOLCHAIN_PATH)/usr/local/lib:$${LD_LIBRARY_PATH:-}" \
	PYTHONHOME="$(TOOLCHAIN_PATH)/usr/local" \
	ZEPHYR_TOOLCHAIN_VARIANT=zephyr \
	ZEPHYR_SDK_INSTALL_DIR="$(TOOLCHAIN_PATH)/opt/zephyr-sdk" \
	CCACHE_DISABLE=1

.PHONY: help check check-toolchain west-update build build-no-scd4x build-scd4x test clean erase flash erase-and-flash

help:
	@echo "Targets: west-update build build-no-scd4x build-scd4x test clean erase flash erase-and-flash"
	@echo "Overrides: TOOLCHAIN_PATH TOOLCHAIN_PYTHON NCS_WORKSPACE BOARD CONF_FILE OVERLAY BUILD_DIR SNR"

check:
	@test -x "$(TOOLCHAIN_PYTHON)" || (echo "Missing TOOLCHAIN_PYTHON: $(TOOLCHAIN_PYTHON)"; exit 1)
	@test -d "$(NCS_WORKSPACE)/.west" || (echo "Invalid NCS_WORKSPACE: $(NCS_WORKSPACE)"; exit 1)
	@test -d "$(NCS_WORKSPACE)/zephyr" || (echo "Missing zephyr dir in NCS_WORKSPACE"; exit 1)
	@test -d "$(NCS_WORKSPACE)/modules/sensirion_drivers" || (echo "Missing SCD4X module in workspace. Run: make west-update"; exit 1)

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

build-no-scd4x: OVERLAY=boards/adafruit_feather_nrf52840.overlay;boards/no_scd4x.overlay
build-no-scd4x: build

build-scd4x: OVERLAY=boards/adafruit_feather_nrf52840.overlay
build-scd4x: build

test:
	@cmake -S tests/unit -B tests/unit/build
	@cmake --build tests/unit/build
	@ctest --test-dir tests/unit/build --output-on-failure

clean:
	@if [ -d "$(BUILD_DIR_ABS)" ]; then \
		cd "$(NCS_WORKSPACE)" && env $(WEST_ENV) $(WEST) build -s "$(APP_DIR)" -d "$(BUILD_DIR_ABS)" -t pristine; \
	else \
		echo "$(BUILD_DIR_ABS) does not exist, nothing to clean."; \
	fi

erase:
	@$(NRFJPROG) -f NRF52 --snr $(SNR) --eraseall

flash:
	@$(NRFJPROG) -f NRF52 --snr $(SNR) --sectorerase --program "$(HEX)" --verify --reset

erase-and-flash: erase flash
