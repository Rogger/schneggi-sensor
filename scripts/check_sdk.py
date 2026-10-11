#!/usr/bin/env python3
"""Reject workspace drift from the project's resolved west manifest."""

from pathlib import Path
import re
import subprocess
import sys


def check_sdk(workspace, manifest):
    # Use the matching toolchain's Python in CI/builds. PyYAML and west are SDK
    # prerequisites; resolving imports checks every SDK module, not only nrf.
    from west.manifest import Manifest

    import yaml

    resolved = Manifest.from_topdir(workspace)
    if yaml.safe_load(Path(resolved.abspath).read_text()) != yaml.safe_load(manifest.read_text()):
        raise ValueError('The selected west workspace uses a different manifest.')
    errors = []
    for project in resolved.projects:
        if project.name == 'manifest' or not resolved.is_active(project):
            continue
        path = workspace / project.path
        try:
            head = subprocess.check_output(
                ['git', '-C', str(path), 'rev-parse', 'HEAD'], text=True).strip()
            # Narrow west updates store fetched tags as manifest-rev without
            # creating local tag refs. This is west's resolved revision.
            expected = project.sha('manifest-rev')
            dirty = subprocess.check_output(
                ['git', '-C', str(path), 'status', '--porcelain', '--untracked-files=no'],
                text=True).strip()
        except subprocess.CalledProcessError:
            errors.append(f'{project.name}: missing revision {project.revision}')
            continue
        if re.fullmatch(r'[0-9a-f]{40}', project.revision) and expected != project.revision:
            errors.append(f'{project.name}: manifest-rev differs from the pinned commit')
        if head != expected:
            errors.append(f'{project.name}: HEAD differs from {project.revision}')
        if dirty:
            errors.append(f'{project.name}: tracked SDK files have local changes')
    if errors:
        raise ValueError('\n'.join(errors) +
                         '\nRestore local SDK edits, if any, then run make west-update '
                         'with the matching SDK toolchain.')


if __name__ == '__main__':
    try:
        check_sdk(Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve())
    except (ValueError, OSError) as error:
        sys.exit(str(error))
