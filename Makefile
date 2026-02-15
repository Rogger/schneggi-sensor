TOOLCHAIN_PATH ?= /home/michael/ncs/toolchains/7795df4459
BUILD_DIR ?= build
BOARD ?= adafruit_feather_nrf52840
CONF_FILE ?= prj_debug.conf
OVERLAY ?= boards/adafruit_feather_nrf52840.overlay
SNR ?= 1050218947
HEX ?= $(BUILD_DIR)/zephyr/merged.hex

WEST ?= west
NRFJPROG ?= nrfjprog

WEST_ENV = \
	PATH="$(TOOLCHAIN_PATH)/bin:$(TOOLCHAIN_PATH)/usr/bin:$(TOOLCHAIN_PATH)/usr/local/bin:$(TOOLCHAIN_PATH)/opt/bin:$(TOOLCHAIN_PATH)/opt/nanopb/generator-bin:$(TOOLCHAIN_PATH)/opt/zephyr-sdk/aarch64-zephyr-elf/bin:$(TOOLCHAIN_PATH)/opt/zephyr-sdk/x86_64-zephyr-elf/bin:$(TOOLCHAIN_PATH)/opt/zephyr-sdk/arm-zephyr-eabi/bin:$$PATH" \
	LD_LIBRARY_PATH="$(TOOLCHAIN_PATH)/lib:$(TOOLCHAIN_PATH)/lib/x86_64-linux-gnu:$(TOOLCHAIN_PATH)/usr/local/lib:$${LD_LIBRARY_PATH:-}" \
	GIT_EXEC_PATH="$(TOOLCHAIN_PATH)/usr/local/libexec/git-core" \
	GIT_TEMPLATE_DIR="$(TOOLCHAIN_PATH)/usr/local/share/git-core/templates" \
	PYTHONHOME="$(TOOLCHAIN_PATH)/usr/local" \
	PYTHONPATH="$(TOOLCHAIN_PATH)/usr/local/lib/python3.8:$(TOOLCHAIN_PATH)/usr/local/lib/python3.8/site-packages" \
	ZEPHYR_TOOLCHAIN_VARIANT=zephyr \
	ZEPHYR_SDK_INSTALL_DIR="$(TOOLCHAIN_PATH)/opt/zephyr-sdk" \
	CCACHE_DISABLE=1

.PHONY: help build test clean erase flash erase-and-flash

help:
	@echo "Targets:"
	@echo "  make build            Build firmware into $(BUILD_DIR)"
	@echo "  make test             Run host unit tests"
	@echo "  make clean            Pristine-clean $(BUILD_DIR)"
	@echo "  make erase            Erase chip (clears Zigbee NVRAM)"
	@echo "  make flash            Flash $(HEX)"
	@echo "  make erase-and-flash  Erase, then flash"
	@echo ""
	@echo "Overrides:"
	@echo "  SNR=<jlink_serial> BUILD_DIR=<dir> CONF_FILE=<file> OVERLAY=<file>"

build:
	@env $(WEST_ENV) $(WEST) build -d $(BUILD_DIR) -b $(BOARD) -- \
		-DNCS_TOOLCHAIN_VERSION=NONE \
		-DWEST_PYTHON="$(TOOLCHAIN_PATH)/usr/local/bin/python3.8" \
		-DCONF_FILE="$(CONF_FILE)" \
		-DDTC_OVERLAY_FILE="$(OVERLAY)"

test:
	@cmake -S tests/unit/zigbee_signal_logic -B tests/unit/zigbee_signal_logic/build
	@cmake --build tests/unit/zigbee_signal_logic/build
	@ctest --test-dir tests/unit/zigbee_signal_logic/build --output-on-failure

clean:
	@if [ -d "$(BUILD_DIR)" ]; then \
		env $(WEST_ENV) $(WEST) build -d $(BUILD_DIR) -t pristine; \
	else \
		echo "$(BUILD_DIR) does not exist, nothing to clean."; \
	fi

erase:
	@$(NRFJPROG) -f NRF52 --snr $(SNR) --eraseall

flash:
	@$(NRFJPROG) -f NRF52 --snr $(SNR) --program $(HEX) --verify --reset

erase-and-flash: erase flash
