# Firmware updates through Home Assistant ZHA

Schneggi uses NCS 2.9.2's Zigbee FOTA client on endpoint 5, cluster `0x0019`,
and MCUboot with ECDSA P-256 signatures and swap using move. Endpoint 1 retains
the sensor clusters. Home Assistant supplies the image and starts installation
through its firmware update entity. No custom ZHA quirk is required by this design.

## One-time installation and signing key

Existing firmware has no OTA bootloader. Install the first OTA-capable
`merged.hex` using the wired programmer. Application-only HEX/BIN files cannot
perform this migration. Back up device flash before migration if retaining the
existing pairing is important; do not use `erase` or `erase-and-flash`.

Generate an ECDSA P-256 private key with MCUboot's `imgtool keygen -t ecdsa-p256
-k /absolute/private/schneggi.pem` (available in the NCS toolchain environment).
Keep this key outside the repository and retain a secure backup. The same key
must sign all subsequent updates for devices installed with it. Losing or
changing it requires another wired installation; bootloader OTA is not supported.

```sh
make build-production OTA_SIGNING_KEY=/absolute/private/schneggi.pem
make flash-production
```

Use `build-production-co2` / `flash-production-co2` for the CO2 board.
Select the correct programmer with `SNR=...`. Flash commands program and erase
only affected sectors. The partition layout keeps ZBOSS network data at
`0xf7000–0xfefff` and product configuration at `0xff000–0xfffff`.
Pairing retention through migration still needs validation on the device.

For a disposable development device, explicitly opt into the publicly known
SDK key:

```sh
make build-debug OTA_ALLOW_TEST_KEY=ON
```

CI artifacts and automated GitHub release assets also use that public key and
are labeled as development firmware, including the production power profiles.
`OTA_ALLOW_TEST_KEY` defaults to `OFF`; ordinary builds
require a private key. Use a separate build directory when changing signing keys.

## Build an update

Increase `CONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION` in the desired profile's `prj_*.conf`,
for example from `1.0.0` to `1.0.1`. Keep debug and production versions aligned
within each hardware variant; CO2 and non-CO2 have separate version histories.
Rebuild with the original signing key:

```sh
make build-production OTA_SIGNING_KEY=/absolute/private/schneggi.pem
make ota-package BUILD_DIR=build_production
make verify-ota BUILD_DIR=build_production OTA_SIGNING_KEY=/absolute/private/schneggi.pem
```

This creates `build_ota_package/index.json` and the corresponding `.zigbee` file.
To create a combined index for both production variants:

```sh
python3 scripts/package_ota.py build_production build_production_co2
```

The tool validates the OTA header, hardware/profile identifiers, image sizes,
the generated DFU payload (one application image with ID 0 and its exact size),
and agreement between MCUboot and Zigbee versions. Conflicting filenames or
existing firmware are rejected before staging files. Firmware files and the index
are installed atomically, so interrupted writes can be retried. Identical firmware
files are left untouched when packaging again.
MCUboot verifies the cryptographic signature on-device before booting a candidate.
The index includes a checksum for ZHA's file-integrity validation.

Versions use `major.minor.patch`, encoded as `(major << 24) | (minor << 16) | patch`.
Major and minor are 8-bit fields; patch is 16-bit. Do not use a `+build` suffix:
Nordic's Zigbee version ignores that suffix. Equal and older versions are refused
by the OTA client. To distribute a fix that restores earlier behavior, release
that code with a new, higher version. Automatic MCUboot trial rollback is separate
from this OTA version policy.

| Profile | Image type |
| --- | --- |
| Production, without CO2 | `0x0101` |
| Production, with CO2 | `0x0102` |
| Debug, without CO2 | `0x8101` |
| Debug, with CO2 | `0x8102` |

Hardware version is `1`. Manufacturer ID `0xFFF1` is a development identifier,
not a CSA assignment to FuZZi. Keep this feed private to your devices; a commercial
release should use an assigned manufacturer ID. The index also matches the
`FuZZi` manufacturer name and `Schneggi Sensor` model. Changing these identifiers
or moving between profiles requires a deliberate migration, normally wired.

Starting with 1.0.2, OTA files and indexes omit the optional minimum/maximum
hardware-version range. NCS 2.9.2's periodic Query Next Image request omits its
hardware version, and zigpy rejects a restricted image when that field is absent.
Removing only the range from the index is insufficient: zigpy also checks the
downloaded OTA header. Compatibility is enforced by manufacturer and image type;
the four image types above are reserved for these hardware-version-1 profiles.
Assign new image types for future incompatible hardware. The client's hardware
version remains 1 for queries triggered by Image Notify. The packager continues
to accept older files with a hardware range of 1–1 for manual notification tests.

## Configure ZHA

Copy the contents of `build_ota_package/` into `/config/zigbee_ota/` on the Home
Assistant host. These are paths **inside Home Assistant**, not on the build PC.
For a different destination, generate the index with `--ha-directory /your/path`.
Merge this into `configuration.yaml`, preserving any existing ZHA configuration:

```yaml
zha:
  zigpy_config:
    ota:
      extra_providers:
        - type: zigpy_local
          index_file: /config/zigbee_ota/index.json
```

Restart Home Assistant to load the provider. After the initial wired migration,
check that ZHA sees endpoint 5's OTA client cluster. Re-interview/reconfigure the
device if needed; if the cached endpoint list remains stale, removal and pairing
again may be necessary. Confirm that the device has a firmware update entity.

The device discovers the OTA server on joining/rebooting, retries discovery every
24 hours if necessary, and queries a discovered server hourly. ZHA can then show
the newer image. Select **Install** on the update entity. Sleepy-device polling
on the non-CO2 variant can delay the start; a missed notification may require
retrying after the device wakes. Its production long polling interval is two
minutes. CO2 firmware from 2.0.0 keeps the receiver on while idle, so it should
accept the notification promptly.

The 2.0.0 CO2 image also changes endpoint 1's power source to DC, removes its
Power Configuration cluster, and increments the endpoint descriptor version.
After updating an existing device, reconfigure or re-interview it in ZHA so the
cached battery entity can be removed. If ZHA keeps the old cluster list, remove
and pair the device again.

To test automatic discovery from 1.0.1, publish the 1.0.2 package and restart
Home Assistant to load its index. Keep the sensor powered and wait for its next
hourly query; do not send the manual Image Notify command. Confirm that ZHA shows
`0x01000002`, then install it. The firmware entity may remain `unknown` until a
query is received. A successfully loaded provider alone does not establish that
the image matches the sensor. Enable ZHA debug logging to inspect the query and
image selection if discovery does not occur.

The battery variant uses a temporary 100 ms polling interval during transfers,
then restores its normal interval on error or completion. The USB-powered CO2
variant leaves its receiver on and does not use the sleepy-device polling controls.
During download, five minutes without an accepted image block causes an abort and
reboot to clear the session. Successful image validation cancels that deadline;
ZBOSS then honors the server's installation time, including indefinite deferral.
A separate five-minute deadline resets the session by rebooting if the server's
Upgrade End response is missing. It checks the ZBOSS state, so an acknowledged
indefinite deferral or scheduled installation does not trigger recovery.
The battery client uses a bounded 30-second turbo-poll window to receive the
Upgrade End response, then returns to normal polling while waiting. The USB
variant keeps receiving continuously. Firmware with OTA resume retains completed
4 KiB flash pages across server aborts, timeout recovery, and device power loss.
Retry **Install** in Home Assistant after the sensor reconnects. Each retry first
downloads and compares the OTA and DFU headers, then jumps to the last committed
page. The incomplete page is downloaded again. Before the first page has been
committed, a retry starts from zero. Home Assistant may initially display 0% until
the device advances its requested offset.

The checkpoint journal uses the existing reserved page at `0xf6000`; neither
MCUboot slots nor Zigbee storage move. Each committed page has a CRC-protected
record written after its data, with a separate commit marker. Incomplete journal
records are ignored. Corrupt stored data, an invalid journal header, a changed
package header, or a different image identity cause a fresh download. Invalid
incoming images discard their checkpoint.
Firmware images must remain immutable for a given manufacturer, image type,
version and size; publish changed contents with a higher version. MCUboot still
verifies the candidate signature before booting it, and trial firmware cannot
write the secondary slot until confirmation. The receiver accepts the same
canonical, single-application DFU packages as `scripts/package_ota.py`.

The firmware currently running on the sensor must include this resume code;
installing it from older firmware uses that older client's restart behavior.
Successful leave events discard retained progress. If a transfer or installation
is active, the device also aborts and reboots after normal leave handling to clear
SDK protocol state; failed leave attempts preserve the session.
Installation briefly interrupts sensor reporting and then rejoins using
persistent network settings.

See [ZHA firmware updates](https://www.home-assistant.io/integrations/zha/#ota-updates-of-zigbee-device-firmware)
and [zigpy provider configuration](https://github.com/zigpy/zigpy/wiki/OTA-Configuration).
An HTTPS index via `zigpy_remote` can be added later. The [release workflow](releases.md)
publishes downloadable assets, including a ZIP for the local ZHA provider; it does
not change Home Assistant remotely.

## Recovery, flash limits, and hardware acceptance

Bench test on 2026-09-28: the USB-powered debug CO2 sensor successfully updated
from 1.0.0 to 1.0.1 through ZHA after a manual Image Notify. The installed version
changed to `0x01000001`, CO2/temperature/humidity reporting resumed, and reporting
and pairing survived a subsequent power cycle. The first installation attempt
failed; the retry completed. On 2026-10-01, ZHA discovered the 1.0.2 image after
its local provider was updated, without a manual Image Notify. Installation again
started on the second attempt, and Home Assistant reported `0x01000002` with
changing sensor readings. This confirms automatic discovery and the basic update
path for that device. The remaining recovery tests below require separate validation.

The 1 MiB internal flash contains a 48 KiB bootloader, two 468 KiB slots, a
reserved page, and the existing 36 KiB Zigbee storage. The image-size check allows
460 KiB including the MCUboot header/signature, reserving pages for move-swap and
its trailer. Keep `pm_static.yml` unchanged for future OTA releases.

MCUboot requests a trial boot. New downloads and image blocks are rejected until
the running image is confirmed, protecting the previous firmware in the secondary
slot. A later OTA query can retry after confirmation. Ten seconds after Zigbee
stack startup, the application confirms the candidate only if required peripheral
devices are ready, plus battery-monitor initialization on non-CO2 firmware.
This is a local startup check, not a test of successful sensor conversions or
coordinator reachability. Failure reboots without confirmation so MCUboot can
revert. A 120-second hardware watchdog is fed by the Zigbee scheduler every 30
seconds to recover from a hang; MCUboot feeds it during swaps. A fault after
confirmation does not automatically
restore the old application.

The health alarm wakes the battery variant every 30 seconds, and its watchdog
stays active during sleep. Previously published current measurements predate OTA
support. Measure idle current again and allow for higher radio current during
updates.

Before deploying to all sensors, test on one device of each hardware variant:

1. Save the current flash, install the initial merged image, and verify sensor
   reporting, retained pairing (or re-pair), and ZHA's OTA endpoint/update entity.
2. Install a higher-version image through ZHA; verify the reported version,
   confirmation, reconnection, and temperature/humidity reporting. Check CO2
   readings on the USB variant and battery reporting on the non-CO2 variant.
3. Interrupt the coordinator during download. Verify timeout recovery and a
   successful retry from the committed offset, with normal polling restored on
   the battery variant. The sensor log reports `Resuming OTA at byte ...`.
4. Interrupt device power during transfer and during the swap. Verify a working
   old or new image and intact pairing data after restart. Retry the interrupted
   download and verify a nonzero requested offset after the headers are received.
5. Offer a wrong-profile image, an older image, a corrupted image, and an image
   signed with a different key. Verify rejection and recovery.
6. Use a deliberately failing trial image on the test device to verify rollback
   after failed startup and watchdog reset; retain wired recovery access.
7. Measure production idle/update current with the debugger disconnected.
8. Drop the Upgrade End response and verify recovery after five minutes. Then
   explicitly defer installation (indefinitely and for more than five minutes)
   and verify that neither case triggers the missing-response recovery.
9. Exercise failed and successful leave attempts during download and deferred
   installation. Failed attempts must preserve the session; successful leaves
   must reset it without executing a stale deferred installation.

Host tests and builds do not establish radio interoperability, physical power-loss
recovery, or battery lifetime. Those checks require the sensor and a running ZHA
installation.
