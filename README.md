# Schneggi sensor 🐌 [![Build debug and release](https://github.com/Rogger/schneggi-sensor/actions/workflows/main.yml/badge.svg)](https://github.com/Rogger/schneggi-sensor/actions/workflows/main.yml)
A Zigbee sensor integrated with Home Assistant for monitoring temperature, humidity, and optional CO2.

- High accuracy temperature and humidity measurements ([Sensirion SHTC3](https://www.sensirion.com/products/catalog/SHTC3/))
- High accuracy CO2 measurement ([Sensirion SCD40](https://sensirion.com/products/catalog/SCD40)) - optional board
- Low power consumption and long battery life for the non-CO2 variant
  - Hardware: nRF52840 chip, low-current linear regulator, battery monitor with on/off capability 
  - Software: Zigbee sleepy end device for the non-CO2 variant
  - Measured: 3uA average over 1 minute without CO2 (before OTA support)
- The non-CO2 variant can be powered with 3V to 6V (JST connector), e.g. 350mAh li-poly.
- The CO2 variant is USB-C powered. It advertises DC power, keeps its Zigbee receiver on while idle, and does not report a battery.
- Small footprint (3,5cm x 3cm )
- Tested with [Home Assistant](https://www.home-assistant.io/) and [SkyConnect](https://www.home-assistant.io/skyconnect/)

## Battery power management

The non-CO2 debug and production profiles enable I2C runtime power management.
The application enables it on the sensor bus; the TWIM driver suspends the
controller and applies its sleep pin configuration between transfers.
Polling, sampling intervals, and SHTC3 measurement accuracy are unchanged.

Failed SHTC3 fetches attempt a sleep command after a 13 ms settling delay,
working around the NCS 2.9.2 driver's missing error-path cleanup without changing
the shared SDK. Cleanup is best-effort: an inaccessible sensor cannot be forced
to sleep. The original fetch error is retained and no failed sample is reported.
The command and conversion timing follow the
[SHTC3 datasheet](https://sensirion.com/file/datasheet_shtc3).

Battery-profile rejoin delays grow from 1 second exponentially to a one-hour
cap (1, 2, 4, ..., 1024, 2048, 3600 seconds). Successful reconnection resets the
backoff. After prolonged outages, recovery may therefore wait up to an hour
before the next attempt, plus commissioning time. USB-powered CO2 profiles keep
their 15-minute cap and do not enable I2C runtime PM or battery monitoring.

Battery measurements run hourly in debug builds and daily in production builds.
If sampling or updating either battery attribute fails, the firmware retries on
the next sensor cycle (one or ten minutes respectively). A successful update
starts the next battery interval. The firmware attempts to disable the divider
on every path and logs cleanup failures.

The firmware restarts if Zigbee startup or scheduling a required measurement,
rejoin, or watchdog alarm fails, so it cannot stay running indefinitely without
that work. Ordinary join failures still use the rejoin backoff described above.
Alarm replacement removes previous instances and uses overflow-safe conversion
for the full supported sampling range of 1–1440 minutes. Repeated Identify
requests keep one blink alarm; scheduling failure stops the LED animation.

Validate on hardware before relying on battery-life estimates: compare at least
an hour of production-profile current capture before/after, with the debugger
disconnected; check repeated sensor reads, transient I2C failures, and recovery
after a prolonged coordinator outage. Host tests cannot verify physical sleep
current or bus suspend/resume behavior.

## Zigbee reporting

ZHA can configure the standard minimum interval, maximum interval, and
reportable change for endpoint 1's Temperature Measurement (`0x0402`) and
Relative Humidity Measurement (`0x0405`) measured-value (`0x0000`) attributes.
Their changes are in hundredths of a degree Celsius and hundredths of a percent
relative humidity. For settings that differ from ZBOSS's defaults, ZBOSS applies
them to every new sensor reading. Without a configuration, or after reporting
is restored to defaults, the firmware retains its 0.1 °C / 1% change thresholds
and 24-hour refresh. A request for ZBOSS's exact default settings (5-second
minimum, no periodic maximum, zero change) is indistinguishable from a reset
and also uses the firmware fallback.

The CO2 Concentration Measurement (`0x040D`) measured value is refreshed on
every sample, so its Zigbee minimum and maximum reporting intervals can be
configured. NCS 2.9.2's ZBOSS treats the single-precision CO2 value as a
changed/unchanged attribute and does not apply a numeric reportable-change
threshold to it. The sensor still samples every minute in debug builds and
every ten minutes in production builds; configuring a shorter reporting
interval does not produce a newer measurement.

## PCB
The PCB was designed with KiCad 7 and manufactured/assembled with JLCPCB. All relevant files can be found in the [repo](hardware)

![base](https://github.com/user-attachments/assets/ae48fca6-d7ca-4260-b0ca-0d7fdca1a1e8)
![co2](https://github.com/user-attachments/assets/a420414e-1857-45f5-8971-0bba9cf12d0e)

## HomeAssistant

![image](https://github.com/user-attachments/assets/fe9de769-8348-4b40-8632-f8fcfae44b9a)

## Case

![image](https://github.com/user-attachments/assets/109c8689-d393-492c-b431-5e4256592e8a)
![image](https://github.com/user-attachments/assets/0dd84a7b-14a9-4d71-ae2d-bca5665277f6)


## Build
This project uses a **west workspace** (manifest in `west.yml`) and a local Makefile wrapper.

### 1) One-time workspace setup
From the project directory:

```bash
west init -l .
make west-update
```

This initializes a west workspace in the parent directory and fetches all required modules
(including `modules/sensirion_drivers` for SCD4X/CO2 builds).

The Makefile uses the parent west workspace when `../.west` exists. To use a
different NCS workspace, pass `NCS_WORKSPACE=/path/to/workspace`.
Builds compare the cached sysbuild source with the selected NCS workspace and
regenerate incompatible project build directories automatically. West's
automatic pristine mode handles other cache incompatibilities.

### 2) Build
Firmware builds now include signed MCUboot/Zigbee OTA support. Supply
`OTA_SIGNING_KEY=/absolute/private/schneggi.pem` to the build commands below.
For development only, use `OTA_ALLOW_TEST_KEY=ON` to opt into the public SDK key.
See [OTA setup, signing, ZHA configuration, and recovery](docs/ota.md).
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

### Build Profiles

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

### 3) Test (host unit tests)

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

Version tags also run the [GitHub release workflow](docs/releases.md), which publishes
clearly labeled public-development-key firmware, a ready-to-copy ZHA OTA ZIP,
and checksums after validation. Releases preserve the separate CO2/non-CO2 firmware versions.

`make clean` accepts only `build` or `build_*` directories inside this project.

### 4) Flash

```bash
make flash-debug
```

For a specific J-Link probe:

```bash
make flash-production-co2 SNR=<your_jlink_serial>
```

### Useful overrides

```bash
make build BUILD_DIR=build_custom
make build CONF_FILE=prj_production_no_scd4x.conf OVERLAY='boards/adafruit_feather_nrf52840.overlay;boards/no_scd4x.overlay'
make build-production-co2 NCS_WORKSPACE=/path/to/workspace
make flash SNR=<your_jlink_serial>
```

### Keep dependencies in sync
After updating `west.yml`:

```bash
make west-update
```

## Resources
- The PCB design was inspired by [Getting Started With nRF52 MCU in a PCB](https://resources.altium.com/p/getting-started-nrf52-mcu-pcb#getting-started-schematics)
- The repos [zigbee-plant-sensor](https://github.com/stanvn/zigbee-plant-sensor) and [b-parasite](https://github.com/rbaron/b-parasite) are a great start
- The [Zyphr driver for SCD40](https://github.com/nobodyguy/sensirion_zephyr_drivers) 
- The [nRF Sniffer](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/protocols/zigbee/tools.html) with [https://www.wireshark.org](Wireshark) is a helpful tool to debug Zigbee communication
- The [ZigBee specification 22 1.0](https://csa-iot.org/wp-content/uploads/2022/01/docs-05-3474-22-0csg-zigbee-specification-1.pdf) and [ZigBee Cluster Library R8](https://zigbeealliance.org/wp-content/uploads/2021/10/07-5123-08-Zigbee-Cluster-Library.pdf)
