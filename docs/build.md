# Build, test and flash

This project uses a **west workspace** (manifest in `west.yml`) and a local Makefile wrapper.
The supported stack is **nRF Connect SDK 3.4.1** with **Zigbee R23 add-on 1.4.0**,
using its R22 behavior mode. SDK and add-on revisions are pinned in `west.yml`;
Zephyr, MCUboot, and other modules come from the SDK manifest. The SCD40 uses
Zephyr's native SCD4X driver. See [migration notes](sdk-upgrade.md).

## 1) One-time workspace setup
Install Nordic's `nrfutil` and the matching toolchain (the Makefile selects the
toolchain associated with this release, rather than the first installed bundle):

```bash
nrfutil install toolchain-manager device
nrfutil toolchain-manager install --ncs-version v3.4.1 --install-dir "$HOME/ncs"
```

From the project directory, initialize the workspace if it does not already exist:

```bash
nrfutil toolchain-manager launch --ncs-version v3.4.1 --install-dir "$HOME/ncs" -- west init -l .
make west-update
```

This initializes a west workspace in the parent directory and fetches all required modules.
For an existing workspace, run `make west-update` with the new toolchain.

The Makefile uses the parent west workspace when `../.west` exists. To use a
different NCS workspace, pass `NCS_WORKSPACE=/path/to/workspace`, or put this
override in the ignored `local.mk`. That workspace must use this checkout's
`west.yml`, including the Zigbee add-on. Builds reject mismatched manifests and
module revisions. An SDK workspace initialized from Nordic's manifest alone
does not include Schneggi's pinned add-on.
Builds compare the cached sysbuild source with the selected NCS workspace and
regenerate incompatible project build directories automatically. West's
automatic pristine mode handles other cache incompatibilities.

## 2) Build
Firmware builds now include signed MCUboot/Zigbee OTA support. Supply
`OTA_SIGNING_KEY=/absolute/private/schneggi.pem` to the build commands below.
For development only, use `OTA_ALLOW_TEST_KEY=ON` to opt into the public SDK key.
See [OTA setup, signing, ZHA configuration, and recovery](ota.md).
Interrupted transfers can resume from the last completed 4 KiB flash page after
retrying Install in Home Assistant, including after a sensor power cycle. The
running firmware must already include resume support.

```bash
cd <project-dir>
make build-debug OTA_ALLOW_TEST_KEY=ON
```

Default output:
- `build_debug/merged.hex`
- `build_debug/schneggi-sensor/zephyr/zephyr.elf`

## Build Profiles

Convenience targets:

- `make build-debug`
  - Debug build without CO2 / SCD4X
- `make build-debug-co2`
  - Debug build with CO2 / SCD4X
- `make build-production`
  - Production build without CO2 / SCD4X
- `make build-production-co2`
  - Production build with CO2 / SCD4X

Equivalent flash targets:

```bash
make flash-debug
make flash-debug-co2
make flash-production
make flash-production-co2
```

For the CO2 production firmware:

```bash
make build-production-co2 OTA_SIGNING_KEY=/absolute/private/schneggi.pem
make flash-production-co2
```

## 3) Test (host unit tests)

```bash
make test
```

The host tests cover reporting thresholds and counter wraparound, battery conversion,
CO2 attribute validation, Zigbee signal handling, OTA behavior for both power profiles,
and rejoin retry/cancellation behavior. They also exercise sensor fetch/read failures,
failed attribute writes, custom reporting and resets, exact fixed-point conversion,
battery ADC/GPIO error cleanup and retry timing, and OTA package validation/index updates.
OTA resume tests simulate resets throughout a download, torn flash writes and
checkpoint commits, corrupted saved data, and maximum-size images.
Runtime tests cover failed startup, alarm exhaustion, cancellation failures,
duplicate reconnect signals, and long scheduling intervals. The Python suite also
checks that PCB rule-check failures prevent manufacturing exports under parallel make.
CI runs them in Debug and Release and builds all four firmware profiles. ADC/GPIO
behavior, network recovery over the radio, and sleep current still require hardware tests.

Version tags also run the [GitHub release workflow](releases.md), which publishes
clearly labeled public-development-key firmware, a ready-to-copy ZHA OTA ZIP,
and checksums after validation. From 2.1.0, CO2 and non-CO2 firmware share one version that matches the tag.

`make clean` accepts only `build` or `build_*` directories inside this project.

## 4) Flash

Flash targets use `nrfutil device`, which requires SEGGER J-Link on the host.
Programming erases only flash sectors touched by the firmware, verifies the
result, and resets the sensor. `make erase` still erases the entire chip.

```bash
make flash-debug
```

For a specific J-Link probe:

```bash
make flash-production-co2 SNR=<your_jlink_serial>
```

## Useful overrides

```bash
make build BUILD_DIR=build_custom
make build CONF_FILE=prj_production_no_scd4x.conf OVERLAY='boards/schneggi_sensor.overlay;boards/no_scd4x.overlay'
make build-production-co2 NCS_WORKSPACE=/path/to/workspace
make flash SNR=<your_jlink_serial>
```

## Keep dependencies in sync
After updating `west.yml`:

```bash
make west-update
```
