import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('package_ota', Path(__file__).parents[1] / 'scripts/package_ota.py')
ota = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ota)


def fixture():
    payload = b'firmware'
    header = struct.pack('<I5HIH32sIHH', 0x0BEEF11E, 0x100, 60, 4, 0xFFF1,
                         0x101, 0x01020003, 2, b'Schneggi', 66 + len(payload), 1, 1)
    return header + struct.pack('<HI', 0, len(payload)) + payload


class OtaTests(unittest.TestCase):
    def test_valid_header(self):
        metadata, payload = ota.parse_ota(fixture())
        self.assertEqual(metadata['file_version'], 0x01020003)
        self.assertEqual(metadata['image_type'], 0x101)
        self.assertEqual(payload, b'firmware')

    def test_invalid_images(self):
        for offset, value in [(0, 0), (6, 56), (8, 0), (10, 0), (12, 0),
                              (52, 0), (56, 2), (58, 2), (60, 1), (62, 0)]:
            with self.subTest(offset=offset):
                data = bytearray(fixture())
                data[offset] = value
                with self.assertRaises(ValueError):
                    ota.parse_ota(data)
        for size in range(len(fixture())):
            with self.assertRaises(ValueError):
                ota.parse_ota(fixture()[:size])

    def test_version(self):
        self.assertEqual(ota.version_number('1.2.3'), 0x01020003)
        self.assertEqual(ota.version_number('255.255.65535'), 0xFFFFFFFF)
        for value in ['1.2.3+4', '256.0.0', '1.256.0', '1.2.65536', '-1.0.0', '1.2']:
            with self.assertRaises(ValueError):
                ota.version_number(value)

    def test_build_payload_and_version_matching(self):
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory)
            app = build / 'sensor' / 'zephyr'
            app.mkdir(parents=True)
            (app / '.config').write_text(
                'CONFIG_ZIGBEE_FOTA=y\nCONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION="1.2.3"\n'
                'CONFIG_ZIGBEE_FOTA_IMAGE_TYPE=0x101\n')
            signed = bytearray(64)
            struct.pack_into('<I', signed, 0, 0x96F3B83D)
            struct.pack_into('<BBH', signed, 20, 1, 2, 3)
            payload = b'package header' + signed
            image = bytearray(fixture()[:60])
            struct.pack_into('<I', image, 52, 66 + len(payload))
            image += struct.pack('<HI', 0, len(payload)) + payload
            (app / 'zephyr.signed.bin').write_bytes(signed)
            (build / 'dfu_multi_image.bin').write_bytes(payload)
            (build / 'image.zigbee').write_bytes(image)
            path, metadata = ota.validate_build(build)
            self.assertEqual(path.name, 'image.zigbee')
            self.assertEqual(metadata['file_version'], 0x01020003)
            ota.package([build], build / 'staged', '/config/zigbee_ota')
            with self.assertRaises(ValueError):
                ota.package([build, build], build / 'duplicates', '/config/zigbee_ota')
            (app / 'zephyr.signed.bin').write_bytes(signed[:-1])
            with self.assertRaises(ValueError):
                ota.validate_build(build)
            (app / 'zephyr.signed.bin').write_bytes(signed)
            (build / 'dfu_multi_image.bin').write_bytes(b'wrong payload')
            with self.assertRaises(ValueError):
                ota.validate_build(build)


if __name__ == '__main__':
    unittest.main()
