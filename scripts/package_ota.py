#!/usr/bin/env python3
"""Validate NCS Zigbee images and stage an index for ZHA's zigpy_local provider."""

import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import struct
import tempfile

PROFILES = {
    0x0101: "production",
    0x0102: "production-co2",
    0x8101: "debug",
    0x8102: "debug-co2",
}


def read_config(path):
    return dict(re.findall(r'^(CONFIG_\w+)=(.*)$', path.read_text(), re.MULTILINE))


def version_number(version):
    match = re.fullmatch(r'(\d+)\.(\d+)\.(\d+)', version)
    if not match:
        raise ValueError("Use major.minor.patch; build suffixes do not affect Zigbee versions")
    major, minor, patch = map(int, match.groups())
    if major > 255 or minor > 255 or patch > 65535:
        raise ValueError("Version exceeds Zigbee/MCUboot field widths")
    return major << 24 | minor << 16 | patch


def parse_ota(data):
    if len(data) < 62:
        raise ValueError("Truncated OTA image")
    magic, header_version, header_size, fields, manufacturer, image_type, version = struct.unpack_from('<I5HI', data)
    size = struct.unpack_from('<I', data, 52)[0]
    if (magic, header_version) != (0x0BEEF11E, 0x0100) or (header_size, fields) not in ((56, 0), (60, 4)):
        raise ValueError("Unexpected OTA header")
    if len(data) < header_size + 6:
        raise ValueError("Truncated OTA image")
    if size != len(data):
        raise ValueError("OTA size mismatch")
    # New images omit the optional hardware range: NCS 2.9.2's periodic
    # Query Next Image omits hardware_version, which zigpy requires when a
    # range is present. Still accept older restricted Schneggi artifacts.
    hardware = {}
    if fields & 4:
        minimum, maximum = struct.unpack_from('<HH', data, 56)
        if (minimum, maximum) != (1, 1):
            raise ValueError("Image is not a supported Schneggi hardware/profile image")
        hardware = {"min_hardware_version": minimum, "max_hardware_version": maximum}
    tag, payload_size = struct.unpack_from('<HI', data, header_size)
    if tag != 0 or payload_size != len(data) - header_size - 6:
        raise ValueError("Invalid OTA image subelement")
    if manufacturer != 0xFFF1 or image_type not in PROFILES:
        raise ValueError("Image is not a supported Schneggi hardware/profile image")
    return {
        "manufacturer_id": manufacturer, "image_type": image_type,
        "file_version": version, "file_size": size,
        **hardware,
        "checksum": "sha3-256:" + hashlib.sha3_256(data).hexdigest(),
        "manufacturer_names": ["FuZZi"], "model_names": ["Schneggi Sensor"],
    }, data[header_size + 6:]


def validate_dfu_payload(payload, signed):
    # NCS's dfu_multi_image_tool.py emits a canonical CBOR header containing
    # {"img": [{"id": 0, "size": len(signed)}]}. Only this single application
    # image is supported by our nRF52840 build. Compare the complete package,
    # not just its suffix: a wrong image ID or length changes what DFU writes.
    size = len(signed)
    if size < 24:
        encoded_size = bytes([size])
    elif size <= 0xFF:
        encoded_size = b'\x18' + struct.pack('>B', size)
    elif size <= 0xFFFF:
        encoded_size = b'\x19' + struct.pack('>H', size)
    else:
        encoded_size = b'\x1a' + struct.pack('>I', size)
    header = b'\xa1\x63img\x81\xa2\x62id\x00\x64size' + encoded_size
    expected = struct.pack('<H', len(header)) + header + signed
    if payload != expected:
        raise ValueError("DFU package must contain exactly the current signed application as image 0")


def validate_build(build):
    configs = list(build.glob('*/zephyr/.config'))
    apps = [p for p in configs if read_config(p).get('CONFIG_ZIGBEE_FOTA') == 'y']
    if len(apps) != 1:
        raise ValueError(f"Expected exactly one OTA application in {build}")
    config = read_config(apps[0])
    # These image types belong to hardware version 1. A future incompatible
    # board must use a new image type rather than rely on optional query data.
    if int(config['CONFIG_ZIGBEE_FOTA_HW_VERSION'], 0) != 1:
        raise ValueError("Schneggi image types are reserved for hardware version 1")
    version = version_number(config['CONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION'].strip('"'))
    image_type = int(config['CONFIG_ZIGBEE_FOTA_IMAGE_TYPE'], 0)
    candidates = []
    for path in build.glob('*.zigbee'):
        metadata, payload = parse_ota(path.read_bytes())
        if (metadata['file_version'], metadata['image_type']) == (version, image_type):
            candidates.append((path, metadata, payload))
    if len(candidates) != 1:
        raise ValueError(f"Expected one OTA image matching the current build in {build}")
    path, metadata, payload = candidates[0]
    if payload != (build / 'dfu_multi_image.bin').read_bytes():
        raise ValueError("OTA payload differs from the generated DFU package")
    signed = (apps[0].parent / 'zephyr.signed.bin').read_bytes()
    if len(signed) < 32 or struct.unpack_from('<I', signed)[0] != 0x96F3B83D:
        raise ValueError("Missing MCUboot image header")
    major, minor, patch = struct.unpack_from('<BBH', signed, 20)
    if (major << 24 | minor << 16 | patch) != version:
        raise ValueError("MCUboot and Zigbee versions differ")
    # Move-swap needs a spare erase page and a trailer page in each 0x75000 slot.
    if len(signed) > 0x73000:
        raise ValueError("Signed image exceeds the fixed OTA slot capacity")
    validate_dfu_payload(payload, signed)
    metadata['changelog'] = f"Schneggi {PROFILES[image_type]} {major}.{minor}.{patch}"
    return path, metadata


def write_atomic(destination, data):
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(mode='wb', dir=destination.parent, delete=False) as file:
            temporary = Path(file.name)
            file.write(data)
        temporary.chmod(0o644)
        temporary.replace(destination)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def package(builds, output, ha_directory):
    if not builds:
        raise ValueError("At least one firmware build is required")
    validated = [validate_build(build) for build in builds]
    identities = [(meta['image_type'], meta['file_version']) for _, meta in validated]
    if len(set(identities)) != len(identities):
        raise ValueError("Duplicate firmware identity")
    names = [source.name for source, _ in validated]
    if len(set(names)) != len(names):
        raise ValueError("Firmware filenames must be unique")
    # Reject conflicts before copying any files into an existing package.
    for source, _ in validated:
        destination = output / source.name
        if destination.exists() and destination.read_bytes() != source.read_bytes():
            raise ValueError(f"Refusing to replace different firmware: {destination}")
    output.mkdir(parents=True, exist_ok=True)
    firmwares = []
    for source, metadata in validated:
        destination = output / source.name
        if not destination.exists():
            write_atomic(destination, source.read_bytes())
        metadata['path'] = str(PurePosixPath(ha_directory) / source.name)
        firmwares.append(metadata)
    # Readers must see either the previous complete index or the new one.
    write_atomic(output / 'index.json', (json.dumps({"firmwares": firmwares}, indent=2) + '\n').encode())


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('builds', nargs='+', type=Path)
    parser.add_argument('--output', type=Path, default=Path('build_ota_package'))
    parser.add_argument('--ha-directory', default='/config/zigbee_ota')
    args = parser.parse_args()
    try:
        package(args.builds, args.output, args.ha_directory)
    except (ValueError, KeyError, OSError, struct.error) as error:
        parser.exit(1, f"OTA packaging failed: {error}\n")
