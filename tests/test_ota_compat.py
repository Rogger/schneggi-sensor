import importlib.util
from pathlib import Path
import struct
import sys
import tempfile
import unittest

from test_package_ota import dfu_package

sys.path.insert(0, str(Path(__file__).parents[1] / 'scripts'))
spec = importlib.util.spec_from_file_location(
    'ota_compat', Path(__file__).parents[1] / 'scripts/ota_compat.py')
compat = importlib.util.module_from_spec(spec)
spec.loader.exec_module(compat)


class OtaCompatibilityTests(unittest.TestCase):
    def fixture(self, root, version='2.0.6'):
        major, minor, patch = map(int, version.split('.'))
        app = root / 'schneggi-sensor' / 'zephyr'
        app.mkdir(parents=True)
        config = app / '.config'
        config.write_text(
            f'CONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION="{version}"\n'
            'CONFIG_ZIGBEE_FOTA=y\nCONFIG_ZIGBEE_FOTA_HW_VERSION=1\n'
            'CONFIG_ZIGBEE_FOTA_MANUFACTURER_ID=0xFFF1\n'
            'CONFIG_ZIGBEE_FOTA_IMAGE_TYPE=0x0102\n'
            'CONFIG_ZIGBEE_FOTA_COMMENT="schneggi-co2"\n')
        signed = bytearray(80)
        struct.pack_into('<I', signed, 0, 0x96F3B83D)
        struct.pack_into('<BBH', signed, 20, major, minor, patch)
        (app / 'zephyr.signed.bin').write_bytes(signed)
        payload = dfu_package(bytes(signed))
        (root / 'dfu_multi_image.bin').write_bytes(payload)
        addon_version = major << 24 | minor << 16 | patch << 8
        header = struct.pack('<I5HIH32sI', 0x0BEEF11E, 0x0100, 56, 0,
                             0xFFF1, 0x0102, addon_version, 2,
                             b'Schneggi', 62 + len(payload))
        source = root / f'FFF1-0102-{addon_version:08X}-schneggi-co2.zigbee'
        source.write_bytes(header + struct.pack('<HI', 0, len(payload)) + payload)
        return config, source

    def test_deployed_version_and_payload_are_preserved(self):
        for version, expected in [('2.0.6', 0x02000006), ('1.0.7', 0x01000007),
                                  ('2.0.0', 0x02000000)]:
            with self.subTest(version=version), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                config, source = self.fixture(root, version)
                before = source.read_bytes()
                destination = compat.convert_image(root, config)
                after = destination.read_bytes()
                self.assertEqual(struct.unpack_from('<I', after, 14)[0], expected)
                self.assertEqual(before[:14] + before[18:], after[:14] + after[18:])
                self.assertEqual(compat.parse_ota(after)[1], (root / 'dfu_multi_image.bin').read_bytes())
                self.assertTrue(destination.is_file())
                if destination != source:
                    self.assertFalse(source.exists())
                # The release packager must accept the migrated build too.
                from package_ota import validate_build
                self.assertEqual(validate_build(root)[1]['file_version'], expected)

    def test_unexpected_payload_is_rejected_without_rewriting(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            config, source = self.fixture(root)
            original = source.read_bytes()
            (root / 'dfu_multi_image.bin').write_bytes(b'wrong image')
            with self.assertRaises(ValueError):
                compat.convert_image(root, config)
            self.assertEqual(source.read_bytes(), original)


if __name__ == '__main__':
    unittest.main()
