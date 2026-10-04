#!/usr/bin/env python3
"""Collect the four validated CI builds into clearly named release assets."""

import argparse
import hashlib
from pathlib import Path
import re
import struct
import zipfile

from package_ota import PROFILES, package, read_config, validate_build


def package_release(builds, output, commit):
    if not re.fullmatch(r'[0-9a-f]{40}', commit):
        raise ValueError('A full source commit SHA is required')
    validated = [(build, *validate_build(build)) for build in builds]
    types = [metadata['image_type'] for _, _, metadata in validated]
    if len(types) != 4 or set(types) != set(PROFILES):
        raise ValueError('A release requires exactly one build of each of the four profiles')
    versions = {meta['image_type']: meta['file_version'] for _, _, meta in validated}
    for production in (0x0101, 0x0102):
        if versions[production] != versions[production | 0x8000]:
            raise ValueError('Debug and production versions must match within each hardware variant')

    # Read all sidecars before writing anything; omit colliding generic names
    # such as merged.hex from the flat GitHub release asset namespace.
    files = {}
    rows = []
    for build, _, metadata in sorted(validated, key=lambda item: item[2]['image_type']):
        profile = PROFILES[metadata['image_type']]
        value = metadata['file_version']
        version = f'{value >> 24}.{(value >> 16) & 255}.{value & 65535}'
        prefix = f'schneggi-{profile}-{version}-public-test-key'
        config = next(path for path in build.glob('*/zephyr/.config')
                      if read_config(path).get('CONFIG_ZIGBEE_FOTA') == 'y')
        for source, suffix in ((build / 'merged.hex', 'merged.hex'),
                               (config.parent / 'zephyr.signed.bin', 'signed.bin'),
                               (build / 'partitions.yml', 'partitions.yml')):
            data = source.read_bytes()
            if not data:
                raise ValueError(f'Empty build artifact: {source}')
            files[f'{prefix}-{suffix}'] = data
        rows.append(f"| {profile} | {version} | `0x{metadata['image_type']:04X}` |")

    output.mkdir(parents=True, exist_ok=False)
    package(builds, output, '/config/zigbee_ota')
    ota_files = sorted(output.iterdir())
    # Fixed ZIP metadata makes identical OTA inputs produce identical archives.
    with zipfile.ZipFile(output / 'schneggi-ota-public-test-key.zip', 'w') as archive:
        for path in ota_files:
            info = zipfile.ZipInfo(path.name)
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            archive.writestr(info, path.read_bytes())
    files['FIRMWARE.md'] = (
        '# Schneggi firmware — public development key\n\n'
        '**These images use the publicly known MCUboot SDK test key. '
        'They are development builds, including the production power profiles.**\n\n'
        f'Source commit: `{commit}`\n\n'
        '| Profile | Firmware version | Zigbee image type |\n'
        '| --- | --- | --- |\n' + '\n'.join(rows) + '\n\n'
        'OTA requires the same image type and signing key as the installed firmware, '
        'and a higher firmware version. The GitHub tag does not override the '
        'per-variant versions shown above.\n\n'
        '- Extract `schneggi-ota-public-test-key.zip` into Home Assistant\'s '
        '`/config/zigbee_ota/`. It contains `index.json` and all four `.zigbee` files.\n'
        '- Use the matching `*-merged.hex` for wired installation; it includes MCUboot.\n'
        '- `*-signed.bin` is the signed application only, not a wired migration image.\n'
        '- `*-partitions.yml` records each build\'s flash layout.\n'
        '- Run `sha256sum --check SHA256SUMS` after downloading all assets.\n\n'
        'See `docs/ota.md` at the source commit for ZHA setup and device validation.\n'
    ).encode()
    for name, data in files.items():
        (output / name).write_bytes(data)
    checksums = ''.join(f'{hashlib.sha256(path.read_bytes()).hexdigest()}  {path.name}\n'
                        for path in sorted(output.iterdir()))
    (output / 'SHA256SUMS').write_text(checksums)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('builds', nargs='+', type=Path)
    parser.add_argument('--output', type=Path, default=Path('build_release_assets'))
    parser.add_argument('--commit', required=True)
    args = parser.parse_args()
    try:
        package_release(args.builds, args.output, args.commit)
    except (ValueError, KeyError, OSError, struct.error) as error:
        parser.exit(1, f'Release packaging failed: {error}\n')
