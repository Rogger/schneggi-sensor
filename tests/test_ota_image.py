from pathlib import Path
import struct
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest

from test_package_ota import dfu_package

scripts = Path(__file__).resolve().parents[1] / 'scripts'
sys.path.insert(0, str(scripts))
from ota_image import build_image
from package_ota import parse_ota, validate_build


def ota_file(version, size, payload, manufacturer, image_type, comment, min_hw, max_hw):
    # Test double for the add-on's OTA_file writer. The firmware build exercises
    # the real writer in the pinned SDK.
    assert size == len(payload)
    hardware = struct.pack('<HH', min_hw, max_hw) if min_hw is not None and max_hw is not None else b''
    header_size = 56 + len(hardware)
    header = struct.pack('<I5HIH32sI', 0x0BEEF11E, 0x0100, header_size,
                         4 if hardware else 0, manufacturer, image_type, version,
                         2, comment.encode('ascii'), header_size + 6 + size)
    return SimpleNamespace(
        filename=f'{manufacturer:04X}-{image_type:04X}-{version:08X}-{comment}.zigbee',
        binary=header + hardware + struct.pack('<HI', 0, size) + payload,
    )


class OtaImageTests(unittest.TestCase):
    def fixture(self, root, version):
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
        struct.pack_into('<BBH', signed, 20, *map(int, version.split('.')))
        (app / 'zephyr.signed.bin').write_bytes(signed)
        (root / 'dfu_multi_image.bin').write_bytes(dfu_package(bytes(signed)))
        return config

    def version_command(self, version):
        return subprocess.run(
            [sys.executable, '-B', str(scripts / 'ota_image.py'), 'version', version],
            capture_output=True, text=True,
        )

    def test_header_uses_the_same_version_as_the_firmware_build(self):
        for version, expected in [('2.0.9', 0x02000009), ('1.0.10', 0x0100000A),
                                  ('2.0.255', 0x020000FF), ('2.0.256', 0x02000100),
                                  ('255.255.65535', 0xFFFFFFFF)]:
            with self.subTest(version=version), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                config = self.fixture(root, version)
                result = self.version_command(version)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(int(result.stdout, 16), expected)
                image = build_image(root, config, ota_file)
                self.assertEqual(image.name, f'FFF1-0102-{expected:08X}-schneggi-co2.zigbee')
                metadata, payload = parse_ota(image.read_bytes())
                self.assertEqual(metadata['file_version'], expected)
                self.assertEqual(payload, (root / 'dfu_multi_image.bin').read_bytes())
                self.assertEqual(validate_build(root)[1]['file_version'], expected)

    def test_firmware_build_rejects_out_of_range_versions(self):
        for version in ('2.0.65536', '256.0.0', '2.256.0', '2.0.0+1'):
            with self.subTest(version=version):
                result = self.version_command(version)
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(result.stdout, '')

    def test_writer_preserves_hardware_limits(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            config = self.fixture(root, '2.0.256')
            with config.open('a') as stream:
                stream.write('CONFIG_ZIGBEE_FOTA_MIN_HW_VERSION=1\n'
                             'CONFIG_ZIGBEE_FOTA_MAX_HW_VERSION=1\n')
            image = build_image(root, config, ota_file)
            metadata, _ = parse_ota(image.read_bytes())
            self.assertEqual(metadata['min_hardware_version'], 1)
            self.assertEqual(metadata['max_hardware_version'], 1)

    def test_rebuild_preserves_the_image_if_validation_fails(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            config = self.fixture(root, '2.0.65535')
            image = build_image(root, config, ota_file)
            original = image.read_bytes()
            self.assertEqual(build_image(root, config, ota_file), image)
            self.assertEqual(image.read_bytes(), original)

            with self.subTest(failure='version mismatch'):
                correct_config = config.read_text()
                config.write_text(correct_config.replace('2.0.65535', '2.0.65534'))
                with self.assertRaisesRegex(ValueError, 'versions differ'):
                    build_image(root, config, ota_file)
                config.write_text(correct_config)
            with self.subTest(failure='payload mismatch'):
                (root / 'dfu_multi_image.bin').write_bytes(b'wrong image')
                with self.assertRaisesRegex(ValueError, 'image 0'):
                    build_image(root, config, ota_file)
            self.assertEqual(list(root.glob('*.zigbee')), [image])
            self.assertEqual(image.read_bytes(), original)
