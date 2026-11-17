#!/usr/bin/env python3
"""Encode the deployed OTA version and build a matching Zigbee image."""

import argparse
import importlib.util
from pathlib import Path
import struct

from package_ota import parse_ota, read_config, validate_application, version_number, write_atomic


def build_image(build, config_path, ota_file):
    config = read_config(config_path)
    version = config['CONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION'].strip('"')
    file_version = version_number(version)
    manufacturer = int(config['CONFIG_ZIGBEE_FOTA_MANUFACTURER_ID'], 0)
    image_type = int(config['CONFIG_ZIGBEE_FOTA_IMAGE_TYPE'], 0)
    comment = config['CONFIG_ZIGBEE_FOTA_COMMENT'].strip('"')
    if len(comment) > 30 or not comment.isascii():
        raise ValueError('OTA comment must be at most 30 ASCII characters')
    min_hw = config.get('CONFIG_ZIGBEE_FOTA_MIN_HW_VERSION')
    max_hw = config.get('CONFIG_ZIGBEE_FOTA_MAX_HW_VERSION')
    payload = (build / 'dfu_multi_image.bin').read_bytes()
    signed = (config_path.parent / 'zephyr.signed.bin').read_bytes()
    validate_application(payload, signed, file_version)

    # OTA_file accepts the packed 32-bit version directly.
    image = ota_file(file_version, len(payload), payload, manufacturer, image_type, comment,
                     int(min_hw, 0) if min_hw else None,
                     int(max_hw, 0) if max_hw else None)
    parse_ota(image.binary)
    destination = build / image.filename
    write_atomic(destination, image.binary)
    return destination


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    version_parser = commands.add_parser('version', help='Print the packed OTA version')
    version_parser.add_argument('version')
    image_parser = commands.add_parser('image', help='Write the Zigbee OTA image')
    image_parser.add_argument('build', type=Path)
    image_parser.add_argument('config', type=Path)
    image_parser.add_argument('generator', type=Path)
    args = parser.parse_args()
    try:
        if args.command == 'version':
            print(f'0x{version_number(args.version):08x}')
        else:
            spec = importlib.util.spec_from_file_location('zb_add_ota_header', args.generator)
            generator = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(generator)
            print(f'Zigbee update created at {build_image(args.build, args.config, generator.OTA_file)}')
    except (ValueError, KeyError, OSError, struct.error) as error:
        parser.exit(1, f'OTA {args.command} failed: {error}\n')
