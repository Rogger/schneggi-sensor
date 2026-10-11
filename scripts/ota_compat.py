#!/usr/bin/env python3
"""Preserve the deployed Schneggi OTA version format with the R23 add-on."""

import argparse
from pathlib import Path
import struct

from package_ota import parse_ota, read_config, validate_dfu_payload, version_number, write_atomic


def convert_image(build, config_path):
    config = read_config(config_path)
    version = config['CONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION'].strip('"')
    major, minor, patch = map(int, version.split('.'))
    if patch > 255:
        raise ValueError('The R23 add-on generator supports patches up to 255')
    addon_version = major << 24 | minor << 16 | patch << 8
    deployed_version = version_number(version)
    manufacturer = int(config['CONFIG_ZIGBEE_FOTA_MANUFACTURER_ID'], 0)
    image_type = int(config['CONFIG_ZIGBEE_FOTA_IMAGE_TYPE'], 0)
    comment = config['CONFIG_ZIGBEE_FOTA_COMMENT'].strip('"')
    source = build / f'{manufacturer:04X}-{image_type:04X}-{addon_version:08X}-{comment}.zigbee'
    data = bytearray(source.read_bytes())
    metadata, payload = parse_ota(data)
    if (metadata['file_version'], metadata['image_type']) != (addon_version, image_type):
        raise ValueError('Generated Zigbee image does not match the build configuration')
    if payload != (build / 'dfu_multi_image.bin').read_bytes():
        raise ValueError('Generated Zigbee image has an unexpected DFU payload')
    validate_dfu_payload(payload, (config_path.parent / 'zephyr.signed.bin').read_bytes())
    struct.pack_into('<I', data, 14, deployed_version)
    destination = build / f'{manufacturer:04X}-{image_type:04X}-{deployed_version:08X}-{comment}.zigbee'
    write_atomic(destination, data)
    if source != destination:
        source.unlink()
    return destination


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('build', type=Path)
    parser.add_argument('config', type=Path)
    args = parser.parse_args()
    convert_image(args.build, args.config)
