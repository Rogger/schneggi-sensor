#!/usr/bin/env python3
"""Select the Nordic toolchain associated with the requested SDK release."""

import json
from pathlib import Path
import sys


def find_toolchain(root, version):
    try:
        catalogs = json.loads((root / 'toolchains.json').read_text())
    except (OSError, ValueError):
        return None
    for catalog in catalogs:
        for entry in catalog.get('toolchains', []):
            if version in entry.get('ncs_versions', []):
                candidate = root / entry['identifier']['bundle_id']
                if candidate.is_dir():
                    return candidate
    return None


if __name__ == '__main__':
    path = find_toolchain(Path(sys.argv[1]), sys.argv[2])
    if path is not None:
        print(path)
