# GitHub firmware releases

The **Release firmware** workflow creates a GitHub Release when a version tag
such as `2.0.3` or `v2.0.3` is pushed. It tests and builds the exact tagged commit,
using the same workflow as branch/PR CI. Debug and Release host tests, all four
firmware builds, signature verification, and OTA packaging must succeed before
anything is published.

All downloadable images use the **public MCUboot SDK development key**. This is
also true for the production power profiles; the profile name is not a security
designation. These images can OTA-update devices installed with that same key
and image type. Devices installed with a private key need separately signed
builds. See [OTA setup and compatibility](ota.md).

## Publish a new version

1. Increase `CONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION` in both debug and production
   configurations for each variant that changed. CO2 and non-CO2 have separate
   version histories; leave an unchanged variant's version alone. Merge the
   changes into `main` after testing.
2. Tag that commit and push the tag, for example:

   ```sh
   git switch main
   git pull --ff-only
   git tag -a 2.0.3 -m 'Schneggi 2.0.3'
   git push origin 2.0.3
   ```

3. Follow **Actions → Release firmware**. Once validation and uploads finish,
   the workflow publishes the release with generated change notes, the source
   commit, per-profile versions, and the public-key notice.

The repository tag labels a source snapshot; it does not override firmware
versions. For example, a `2.0.3` release may contain CO2 `2.0.3` and non-CO2
`1.0.5`. Increase each affected variant's firmware version before tagging, or
existing devices running that version will refuse the OTA update.

Alternatively, use **Actions → Release firmware → Run workflow**, select `main`,
and enter an existing tag. It must point to a commit containing the release
packaging scripts. This creates a release for a tag that has not yet been
published. Tags support `major.minor.patch` with an optional `v` prefix; suffixes
such as `-rc1` are not supported by this workflow.

## Downloadable assets

| Asset | Purpose |
| --- | --- |
| `schneggi-ota-public-test-key.zip` | All four `.zigbee` images and `index.json`, ready to extract into HA's `/config/zigbee_ota/` |
| Individual `.zigbee` files and `index.json` | The same local-provider OTA package as separate downloads |
| `schneggi-<profile>-<version>-public-test-key-merged.hex` | Wired installation including MCUboot |
| `schneggi-<profile>-<version>-public-test-key-signed.bin` | Signed application only |
| `schneggi-<profile>-<version>-public-test-key-partitions.yml` | Flash layout for each build |
| `FIRMWARE.md` | Source commit, versions, signing-key notice, and installation guidance |
| `SHA256SUMS` | SHA-256 checksums of all the other assets |

The same flat set of files is available from normal CI under the Actions artifact
`development-firmware-public-test-key`. No local build environment is needed to
download and use them. Home Assistant still needs the [local OTA provider](ota.md#configure-zha)
configured and the package copied to its configuration directory.

## Failed runs and retries

A failed build does not create a release. Publication starts with a draft and
only makes it public after all uploads succeed. A rerun may replace matching
assets in that draft to complete a failed upload. It refuses to modify an already
published release: use a new tag and bump affected firmware versions for fixes.
Existing draft release notes are preserved; newly created drafts include the
firmware notice and GitHub-generated change notes. Do not move a tag during a run;
the workflow verifies that it still points to the commit that was built.

Only the publishing job has `contents: write`; test/build jobs have read access.
The built-in `GITHUB_TOKEN` handles publication, so no extra token or signing
secret is required for these public-key development releases.
