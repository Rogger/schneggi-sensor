# SDK 3.4.1 migration

Schneggi now uses nRF Connect SDK 3.4.1 and Zigbee R23 add-on 1.4.0. Both
repositories are pinned to commits in `west.yml`. The SDK supplies the compatible
Zephyr, MCUboot, radio libraries, and Python dependencies. Use the matching
Nordic toolchain; the Makefile reads `toolchains.json` to select it.

## Changes

- The board uses Zephyr's hardware model v2, with target
  `schneggi_sensor/nrf52840`. Its pin assignments and DCDC setting are retained.
- Zigbee is supplied by the separate add-on and uses R22 behavior mode.
  Nordic's KRKNWK-21014 radio sleep workaround is applied after steering,
  trust-center rejoin, and reboot signals.
- The SCD40 uses the SDK's native SCD4X driver. Startup applies the original
  574 m altitude compensation, zero temperature offset, and enabled automatic
  calibration. Override altitude with `CONFIG_APP_SCD4X_ALTITUDE`. Calibration
  or first-sample failure prevents trial-image confirmation. Each attempt polls
  the sensor's data-ready status with CRC validation and a ten-second timeout,
  then fetches the first measurement through the native driver before exposing
  its cache. Confirmed firmware starts Zigbee first and retries calibration and
  first-sample acquisition on the main thread with a 30-second delay between
  failed attempts.
  See SCD4x datasheet sections 3.6.2 and 3.9.2 in the
  [Sensirion datasheet](https://sensirion.com/media/documents/48C4B7FB/67FE0194/CD_DS_SCD4x_Datasheet_D1.pdf).
- MCUboot keeps ECDSA P-256 signing and swap using move. The fixed flash layout
  in `pm_static.yml` is retained, including the OTA checkpoint page and Zigbee
  storage. Partition Manager remains explicitly enabled; it is deprecated in
  this SDK. A later migration to DTS partitions must reproduce every address
  and size before removing this file.
- Schneggi keeps its durable OTA resume implementation. The add-on's optional
  resume feature is disabled. The build passes Schneggi's packed version directly
  to the add-on's OTA header writer and uses the same encoder for the application's
  advertised version. This preserves the full 16-bit patch field.
  Both variants advance to 2.1.0 (CO2 from 2.0.8, battery from 1.0.9) and share
  one version from now on. The image
  types, manufacturer ID, and signing-key policy are retained.
- Flash targets use `nrfutil device` with sector erase, verification, and reset.
- CI uses Ubuntu 24.04, the SDK 3.4.1 container pinned by digest, and updated
  GitHub Actions pinned to commits. Release assets include the resolved SDK
  manifest and its checksum.

## Local setup

Install the matching toolchain and fetch the new dependencies as described in
[build.md](build.md). Existing NCS 2.9.2 build directories need
regeneration. The Makefile handles a changed sysbuild workspace automatically;
use a fresh build directory when changing signing keys.

For a separate workspace, clone this repository into a new parent directory,
initialize west there, and set `NCS_WORKSPACE` to that parent. Keep `west.yml`
linked to the checkout being built. The build's dependency check rejects an
old SDK, a mismatched manifest, or modified tracked SDK files before invoking
CMake. Untracked build files in SDK repositories do not affect this check.
If a narrow west update left a required release tag out of the local clone,
the check resolves that tag from the manifest's remote.
Missing toolchains fail explicitly before the build environment is set up.

## Hardware validation

Host tests and successful builds do not establish hardware compatibility.
Validate on one battery device and one USB CO2 device before rollout:

1. Save flash and retain wired recovery access. Use the original signing key
   for devices that already have an OTA bootloader.
2. Update through ZHA from CO2 2.0.8 and battery 1.0.9 to 2.1.0.
   Check the current version, pairing, every sensor endpoint, calibration, and
   reporting after reboot.
3. Exercise interrupted downloads, power loss, deferred installation, trial
   confirmation, and rollback as described in [OTA acceptance](ota.md#recovery-flash-limits-and-hardware-acceptance).
4. Measure idle current without the debugger, including after leave/rejoin and
   failed steering. Check long polling, sensor-bus suspend, and battery reporting.

The new ZBOSS migrates its NVRAM format on first startup. Retaining the flash
addresses prevents relocation, but does not prove that reverting to the old
stack can read the migrated data. Check pairing retention during rollback on
the bench before relying on that recovery path.

## Upstream references

- [SDK 3.4.1 release](https://github.com/nrfconnect/sdk-nrf/releases/tag/v3.4.1)
- [Zigbee add-on 1.4.0 release notes](https://github.com/nrfconnect/ncs-zigbee/blob/v1.4.0/docs/release-notes.rst)
- [R22 to R23 migration and NVRAM changes](https://github.com/nrfconnect/ncs-zigbee/blob/v1.4.0/docs/migration.rst)
- [Known issues and radio sleep workaround](https://github.com/nrfconnect/ncs-zigbee/blob/v1.4.0/docs/known_issues.rst)
- [Partition guidance](https://github.com/nrfconnect/ncs-zigbee/blob/v1.4.0/docs/configuring.rst)
