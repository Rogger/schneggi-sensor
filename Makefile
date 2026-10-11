# Minimal Makefile for Zephyr/NCS app builds

# Optional local overrides (not committed)
-include local.mk

# --- Configurable paths ---
NCS_VERSION := v3.4.1
NCS_TOOLCHAINS_DIR ?= $(HOME)/ncs/toolchains
TOOLCHAIN_PATH ?= $(shell python3 "$(APP_DIR)/scripts/find_toolchain.py" "$(NCS_TOOLCHAINS_DIR)" "$(NCS_VERSION)")
TOOLCHAIN_PYTHON ?= $(if $(strip $(TOOLCHAIN_PATH)),$(firstword $(wildcard $(TOOLCHAIN_PATH)/usr/local/bin/python3.*) $(wildcard $(TOOLCHAIN_PATH)/usr/local/bin/python3)))
XDG_CACHE_HOME ?= /tmp/codex-cache

# --- Build config ---
BOARD ?= schneggi_sensor/nrf52840
CONF_FILE ?= prj_debug_no_scd4x.conf
OVERLAY ?= boards/schneggi_sensor.overlay;boards/no_scd4x.overlay
BUILD_DIR ?= build_default
OTA_ALLOW_TEST_KEY ?= OFF
OTA_SIGNING_KEY ?=
APP_DIR := $(CURDIR)
PARENT_WORKSPACE := $(abspath $(APP_DIR)/..)
NCS_WORKSPACE ?= $(if $(wildcard $(PARENT_WORKSPACE)/.west),$(PARENT_WORKSPACE),$(HOME)/ncs/$(NCS_VERSION))
BUILD_DIR_ABS = $(if $(filter /%,$(BUILD_DIR)),$(BUILD_DIR),$(APP_DIR)/$(BUILD_DIR))
HEX ?= $(BUILD_DIR_ABS)/merged.hex

# --- Flash config ---
SNR ?= 1050218947
NRFUTIL ?= nrfutil
WEST ?= west

WEST_ENV = \
	PATH="$(TOOLCHAIN_PATH)/bin:$(TOOLCHAIN_PATH)/usr/bin:$(TOOLCHAIN_PATH)/usr/local/bin:$(TOOLCHAIN_PATH)/opt/bin:$$PATH" \
	LD_LIBRARY_PATH="$(TOOLCHAIN_PATH)/lib:$(TOOLCHAIN_PATH)/lib/x86_64-linux-gnu:$(TOOLCHAIN_PATH)/usr/local/lib:$${LD_LIBRARY_PATH:-}" \
	GIT_EXEC_PATH="/usr/lib/git-core" \
	PYTHONHOME="$(TOOLCHAIN_PATH)/usr/local" \
	XDG_CACHE_HOME="$(XDG_CACHE_HOME)" \
	ZEPHYR_TOOLCHAIN_VARIANT=zephyr \
	ZEPHYR_SDK_INSTALL_DIR="$(TOOLCHAIN_PATH)/opt/zephyr-sdk" \
	CCACHE_DISABLE=1

.PHONY: help check check-toolchain prepare-build-cache west-update build build-no-scd4x build-scd4x build-debug build-debug-co2 build-production build-production-co2 test clean erase flash erase-and-flash flash-debug flash-debug-co2 flash-production flash-production-co2

help:
	@echo "Targets: west-update build build-no-scd4x build-scd4x build-debug build-debug-co2 build-production build-production-co2 test clean erase flash erase-and-flash flash-debug flash-debug-co2 flash-production flash-production-co2"
	@echo "Overrides: NCS_TOOLCHAINS_DIR TOOLCHAIN_PATH TOOLCHAIN_PYTHON XDG_CACHE_HOME NCS_WORKSPACE BOARD CONF_FILE OVERLAY BUILD_DIR NRFUTIL SNR"
	@echo "OTA: ota-package verify-ota; signing: OTA_SIGNING_KEY=/absolute/private/key.pem (or OTA_ALLOW_TEST_KEY=ON for development)"

check: check-toolchain
	@test -d "$(NCS_WORKSPACE)/zephyr" || (echo "Missing zephyr dir in NCS_WORKSPACE: $(NCS_WORKSPACE). Run: make west-update"; exit 1)
	@test -d "$(NCS_WORKSPACE)/ncs-zigbee" || (echo "Missing Zigbee add-on. Run: make west-update"; exit 1)
	@env $(WEST_ENV) "$(TOOLCHAIN_PYTHON)" scripts/check_sdk.py "$(NCS_WORKSPACE)" "$(APP_DIR)/west.yml"

check-toolchain:
	@test -n "$(TOOLCHAIN_PATH)" && test -d "$(TOOLCHAIN_PATH)/opt/zephyr-sdk" || (echo "Missing Nordic toolchain for $(NCS_VERSION). Install it with nrfutil toolchain-manager or set TOOLCHAIN_PATH."; exit 1)
	@test -x "$(TOOLCHAIN_PYTHON)" || (echo "Missing TOOLCHAIN_PYTHON: $(TOOLCHAIN_PYTHON)"; exit 1)
	@mkdir -p "$(XDG_CACHE_HOME)"
	@test -d "$(NCS_WORKSPACE)/.west" || (echo "Invalid NCS_WORKSPACE: $(NCS_WORKSPACE)"; exit 1)

west-update: check-toolchain
	@cd "$(NCS_WORKSPACE)" && env $(WEST_ENV) $(WEST) update

prepare-build-cache:
	@if [ -f "$(BUILD_DIR_ABS)/CMakeCache.txt" ]; then \
		cached_source=$$(sed -n 's/^CMAKE_HOME_DIRECTORY:INTERNAL=//p' "$(BUILD_DIR_ABS)/CMakeCache.txt"); \
		expected_source="$(realpath $(NCS_WORKSPACE)/zephyr/share/sysbuild)"; \
		if [ -n "$$cached_source" ] && [ "$$cached_source" != "$$expected_source" ]; then \
			echo "Build cache uses $$cached_source; regenerating for $$expected_source"; \
			$(MAKE) --no-print-directory clean BUILD_DIR="$(BUILD_DIR)"; \
		fi; \
	fi

build: check prepare-build-cache
	@cd "$(NCS_WORKSPACE)" && env $(WEST_ENV) $(WEST) build \
		--pristine=auto \
		--sysbuild \
		-s "$(APP_DIR)" -d "$(BUILD_DIR_ABS)" -b "$(BOARD)" -- \
		-DNCS_TOOLCHAIN_VERSION=NONE \
		-DBOARD_ROOT="$(APP_DIR)" \
		-DOTA_ALLOW_TEST_KEY="$(OTA_ALLOW_TEST_KEY)" \
		-DSB_CONFIG_BOOT_SIGNATURE_KEY_FILE='"$(if $(OTA_SIGNING_KEY),$(OTA_SIGNING_KEY),$(NCS_WORKSPACE)/bootloader/mcuboot/root-ec-p256.pem)"' \
		-DWEST_PYTHON="$(TOOLCHAIN_PYTHON)" \
		-DCONF_FILE="$(CONF_FILE)" \
		-DDTC_OVERLAY_FILE="$(OVERLAY)"

build-no-scd4x: CONF_FILE=prj_debug_no_scd4x.conf
build-no-scd4x: BUILD_DIR=build_no_scd4x
build-no-scd4x: OVERLAY=boards/schneggi_sensor.overlay;boards/no_scd4x.overlay
build-no-scd4x: build

build-scd4x: CONF_FILE=prj_debug.conf
build-scd4x: BUILD_DIR=build_scd4x
build-scd4x: OVERLAY=boards/schneggi_sensor.overlay
build-scd4x: build

build-debug: BUILD_DIR=build_debug
build-debug: CONF_FILE=prj_debug_no_scd4x.conf
build-debug: OVERLAY=boards/schneggi_sensor.overlay;boards/no_scd4x.overlay
build-debug: build

build-debug-co2: BUILD_DIR=build_debug_co2
build-debug-co2: CONF_FILE=prj_debug.conf
build-debug-co2: OVERLAY=boards/schneggi_sensor.overlay
build-debug-co2: build

build-production: BUILD_DIR=build_production
build-production: CONF_FILE=prj_production_no_scd4x.conf
build-production: OVERLAY=boards/schneggi_sensor.overlay;boards/no_scd4x.overlay
build-production: build

build-production-co2: BUILD_DIR=build_production_co2
build-production-co2: CONF_FILE=prj_production.conf
build-production-co2: OVERLAY=boards/schneggi_sensor.overlay
build-production-co2: build

test:
	@python3 -m unittest discover -s tests -p 'test_*.py'
	@cmake -S tests/unit -B tests/unit/build
	@cmake --build tests/unit/build
	@ctest --test-dir tests/unit/build --output-on-failure

.PHONY: ota-package verify-ota
ota-package:
	@python3 scripts/package_ota.py "$(BUILD_DIR_ABS)"

verify-ota: check-toolchain
	@env $(WEST_ENV) "$(TOOLCHAIN_PYTHON)" "$(NCS_WORKSPACE)/bootloader/mcuboot/scripts/imgtool.py" verify \
		-k "$(if $(OTA_SIGNING_KEY),$(OTA_SIGNING_KEY),$(NCS_WORKSPACE)/bootloader/mcuboot/root-ec-p256.pem)" \
		"$(BUILD_DIR_ABS)/$(notdir $(APP_DIR))/zephyr/zephyr.signed.bin"

clean:
	@case "$(realpath $(BUILD_DIR_ABS))" in \
		"" ) ;; \
		"$(realpath $(APP_DIR))"/build_*|"$(realpath $(APP_DIR))"/build) ;; \
		* ) echo "Refusing to clean outside a project build directory: $(BUILD_DIR_ABS)"; exit 1 ;; \
	esac
	@if [ -d "$(BUILD_DIR_ABS)" ]; then \
		rm -rf "$(BUILD_DIR_ABS)"; \
		echo "Removed $(BUILD_DIR_ABS)"; \
	else \
		echo "$(BUILD_DIR_ABS) does not exist, nothing to clean."; \
	fi

erase:
	@$(NRFUTIL) device erase --family nrf52 --serial-number $(SNR) --all

flash:
	@$(NRFUTIL) device program --family nrf52 --serial-number $(SNR) --firmware "$(HEX)" \
		--options chip_erase_mode=ERASE_RANGES_TOUCHED_BY_FIRMWARE,verify=VERIFY_READ,reset=RESET_SYSTEM

erase-and-flash: erase
	@$(MAKE) flash

flash-debug: BUILD_DIR=build_debug
flash-debug: flash

flash-debug-co2: BUILD_DIR=build_debug_co2
flash-debug-co2: flash

flash-production: BUILD_DIR=build_production
flash-production: flash

flash-production-co2: BUILD_DIR=build_production_co2
flash-production-co2: flash
