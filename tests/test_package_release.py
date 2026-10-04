import hashlib
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import zipfile

sys.path.insert(0, str(Path(__file__).parents[1] / 'scripts'))
import package_release as release
from test_package_ota import dfu_package


def make_build(root, image_type, version):
    build = root / f'build_{image_type:04x}'
    app = build / 'sensor/zephyr'
    app.mkdir(parents=True)
    app.joinpath('.config').write_text(
        'CONFIG_ZIGBEE_FOTA=y\nCONFIG_ZIGBEE_FOTA_HW_VERSION=1\n'
        f'CONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION="{version}"\n'
        f'CONFIG_ZIGBEE_FOTA_IMAGE_TYPE={image_type}\n'
    )
    major, minor, patch = map(int, version.split('.'))
    signed = bytearray(32)
    struct.pack_into('<I', signed, 0, 0x96F3B83D)
    struct.pack_into('<BBH', signed, 20, major, minor, patch)
    app.joinpath('zephyr.signed.bin').write_bytes(signed)
    payload = dfu_package(signed)
    build.joinpath('dfu_multi_image.bin').write_bytes(payload)
    header = struct.pack('<I5HIH32sI', 0x0BEEF11E, 0x100, 56, 0, 0xFFF1,
                         image_type, major << 24 | minor << 16 | patch,
                         2, b'Schneggi', 62 + len(payload))
    build.joinpath(f'{image_type:04X}.zigbee').write_bytes(
        header + struct.pack('<HI', 0, len(payload)) + payload)
    build.joinpath('merged.hex').write_text(f'hex for {image_type}')
    build.joinpath('partitions.yml').write_text(f'layout for {image_type}')
    return build


class ReleaseTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.builds = [make_build(self.root, image_type, '2.0.2' if image_type & 2 else '1.0.4')
                       for image_type in release.PROFILES]
        self.output = self.root / 'assets'
        self.commit = 'a' * 40

    def package(self, builds=None):
        release.package_release(self.builds if builds is None else builds, self.output, self.commit)

    def test_complete_release_has_unique_files_ota_zip_and_checksums(self):
        self.package()
        self.assertEqual(len(list(self.output.iterdir())), 20)
        for profile in release.PROFILES.values():
            version = '2.0.2' if profile.endswith('co2') else '1.0.4'
            for suffix in ('merged.hex', 'signed.bin', 'partitions.yml'):
                self.assertTrue((self.output / f'schneggi-{profile}-{version}-public-test-key-{suffix}').is_file())
        entries = json.loads((self.output / 'index.json').read_text())['firmwares']
        self.assertEqual(len(entries), 4)
        for entry in entries:
            filename = Path(entry['path']).name
            self.assertEqual(entry['path'], f'/config/zigbee_ota/{filename}')
            data = (self.output / filename).read_bytes()
            self.assertEqual(entry['checksum'], 'sha3-256:' + hashlib.sha3_256(data).hexdigest())
        with zipfile.ZipFile(self.output / 'schneggi-ota-public-test-key.zip') as archive:
            self.assertEqual(set(archive.namelist()), {'index.json', '0101.zigbee', '0102.zigbee', '8101.zigbee', '8102.zigbee'})
            for name in archive.namelist():
                self.assertEqual(archive.read(name), (self.output / name).read_bytes())
        manifest = (self.output / 'SHA256SUMS').read_text().splitlines()
        self.assertEqual(len(manifest), 19)
        for line in manifest:
            digest, name = line.split('  ')
            self.assertEqual(digest, hashlib.sha256((self.output / name).read_bytes()).hexdigest())
        notes = (self.output / 'FIRMWARE.md').read_text()
        self.assertIn(self.commit, notes)
        self.assertIn('publicly known MCUboot SDK test key', notes)
        self.assertIn('| debug-co2 | 2.0.2 | `0x8102` |', notes)

    def test_missing_duplicate_and_mismatched_profiles_fail_before_output(self):
        for builds in (self.builds[:-1], self.builds + self.builds[:1], [self.builds[0]] * 4):
            with self.subTest(builds=builds), self.assertRaises(ValueError):
                self.package(builds)
            self.assertFalse(self.output.exists())
        other = make_build(self.root / 'other', 0x8102, '2.0.3')
        with self.assertRaisesRegex(ValueError, 'versions must match'):
            self.package(self.builds[:-1] + [other])
        self.assertFalse(self.output.exists())

    def test_missing_sidecar_leaves_no_partial_release(self):
        (self.builds[0] / 'merged.hex').unlink()
        with self.assertRaises(FileNotFoundError):
            self.package()
        self.assertFalse(self.output.exists())

    def test_existing_output_is_not_mixed_with_new_artifacts(self):
        self.output.mkdir()
        (self.output / 'old-file').write_text('preserve')
        with self.assertRaises(FileExistsError):
            self.package()
        self.assertEqual(list(self.output.iterdir()), [self.output / 'old-file'])

    def test_source_commit_is_required(self):
        with self.assertRaises(ValueError):
            release.package_release(self.builds, self.output, 'main')
        self.assertFalse(self.output.exists())


if __name__ == '__main__':
    unittest.main()
