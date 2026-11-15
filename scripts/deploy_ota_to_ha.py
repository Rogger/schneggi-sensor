#!/usr/bin/env python3
"""Deploy a release OTA ZIP to a remote Home Assistant ZHA provider directory."""

import argparse
import getpass
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shlex
import shutil
import subprocess
import sys
import tempfile
import uuid
import zipfile


def validate_archive(archive_path, ota_subdir):
    """Check the ZIP's index, file sizes, and checksums before asking for a password."""
    with zipfile.ZipFile(archive_path) as archive:
        names = set(archive.namelist())
        if "index.json" not in names:
            raise ValueError("OTA ZIP has no index.json")
        index = json.loads(archive.read("index.json"))
        firmwares = index.get("firmwares", [])
        if not firmwares:
            raise ValueError("OTA index has no firmware entries")

        expected_names = set()
        expected_prefix = f"/config/{ota_subdir}/"
        for image in firmwares:
            ota_path = image.get("path", "")
            if not ota_path.startswith(expected_prefix):
                raise ValueError(f"Unexpected OTA path in index: {ota_path}")
            name = PurePosixPath(ota_path).name
            if not name.endswith(".zigbee") or name in expected_names:
                raise ValueError(f"Invalid or duplicate firmware filename: {name}")
            expected_names.add(name)
            if name not in names:
                raise ValueError(f"OTA ZIP is missing {name}")
            data = archive.read(name)
            if len(data) != image.get("file_size"):
                raise ValueError(f"Size mismatch for {name}")
            algorithm, separator, expected_hash = image.get("checksum", "").partition(":")
            if algorithm != "sha3-256" or not separator:
                raise ValueError(f"Unsupported checksum for {name}")
            if hashlib.sha3_256(data).hexdigest() != expected_hash:
                raise ValueError(f"Checksum mismatch for {name}")

        if names != expected_names | {"index.json"}:
            raise ValueError("OTA ZIP contains unexpected or missing files")
        return len(firmwares)


def prompt_password(mode):
    """Prompt once, preferring a desktop dialog when requested or available."""
    has_display = bool(os.environ.get("DISPLAY") or os.environ.get("WAYLAND_DISPLAY"))
    zenity = shutil.which("zenity")
    use_gui = mode == "gui" or (mode == "auto" and has_display and zenity)

    if use_gui:
        if not zenity:
            raise RuntimeError("GUI password prompt requested, but zenity is not installed")
        result = subprocess.run(
            [zenity, "--password", "--title=Deploy Schneggi OTA",
             "--text=Enter your TrueNAS SSH/sudo password once."],
            text=True,
            capture_output=True,
        )
        if result.returncode:
            detail = result.stderr.strip()
            if mode == "auto" and detail:
                print("Desktop prompt unavailable; falling back to the hidden terminal prompt.", file=sys.stderr)
                password = getpass.getpass("TrueNAS SSH/sudo password: ")
            else:
                raise RuntimeError("Password dialog cancelled" + (f": {detail}" if detail else ""))
        else:
            password = result.stdout.rstrip("\r\n")
    else:
        password = getpass.getpass("TrueNAS SSH/sudo password: ")

    if not password:
        raise RuntimeError("Empty password; nothing was copied")
    return password


def build_remote_script():
    return r'''set -eu
zip=$1
config_dir=$2
ota_subdir=$3
container=$4
ota_dir="$config_dir/$ota_subdir"
stage=$(mktemp -d /tmp/schneggi-ota-stage.XXXXXX)
trap 'rm -rf "$stage"' EXIT
test -f "$zip"
test -d "$config_dir"
test -d "$ota_dir"
test -f "$ota_dir/index.json"
unzip -o -q "$zip" -d "$stage"
python3 - "$stage" "$ota_subdir" <<'CHECK'
import hashlib, json, pathlib, sys
d = pathlib.Path(sys.argv[1])
subdir = sys.argv[2]
doc = json.loads((d / "index.json").read_text())
items = doc.get("firmwares", [])
assert items, "OTA index has no firmware entries"
expected = set()
prefix = f"/config/{subdir}/"
for item in items:
    ota_path = item["path"]
    assert ota_path.startswith(prefix), ota_path
    name = pathlib.PurePosixPath(ota_path).name
    assert name.endswith(".zigbee") and name not in expected, name
    expected.add(name)
    p = d / name
    assert p.is_file(), f"Missing {p}"
    assert p.stat().st_size == item["file_size"], f"Size mismatch: {name}"
    algorithm, sep, digest = item["checksum"].partition(":")
    assert algorithm == "sha3-256" and sep, f"Unsupported checksum: {name}"
    assert hashlib.sha3_256(p.read_bytes()).hexdigest() == digest, f"Checksum mismatch: {name}"
    print(f"Verified {name} ({item['file_version']:#010x})")
assert {p.name for p in d.glob("*.zigbee")} == expected, "Unexpected firmware files in ZIP"
CHECK
backup="$ota_dir/index.json.bak-$(date -u +%Y%m%dT%H%M%SZ)"
cp -p "$ota_dir/index.json" "$backup"
printf 'Saved previous OTA index: %s\n' "$backup"
for file in "$stage"/*.zigbee; do install -m 0644 "$file" "$ota_dir/"; done
install -m 0644 "$stage/index.json" "$ota_dir/index.json"
rm -f "$zip"
docker restart "$container"
status=$(docker inspect --format '{{.State.Status}}' "$container")
printf 'Home Assistant container status: %s\n' "$status"
test "$status" = running
'''


def deploy(args):
    ota_subdir = PurePosixPath(args.ota_subdir)
    if ota_subdir.is_absolute() or ".." in ota_subdir.parts or not ota_subdir.parts:
        raise ValueError("OTA subdirectory must be a safe relative path")
    if not args.archive.is_file():
        raise ValueError(f"OTA ZIP not found: {args.archive}")

    count = validate_archive(args.archive, ota_subdir.as_posix())
    print(f"Validated {count} firmware image(s) in {args.archive}")
    password = prompt_password(args.password_prompt)
    secret = password.encode() + b"\n"
    del password

    remote_zip = f"/tmp/schneggi-ota-{uuid.uuid4().hex}.zip"
    ssh_options = [
        "-o", "PreferredAuthentications=publickey,password",
        "-o", "NumberOfPasswordPrompts=1",
        "-o", "StrictHostKeyChecking=yes",
    ]

    with tempfile.TemporaryDirectory(prefix="schneggi-ota-auth-") as temp_dir:
        os.chmod(temp_dir, 0o700)
        secret_file = Path(temp_dir) / "password"
        secret_file.write_bytes(secret)
        os.chmod(secret_file, 0o600)
        del secret

        askpass = Path(temp_dir) / "askpass"
        askpass.write_text(
            "#!/usr/bin/env python3\n"
            "import os, sys\n"
            "sys.stdout.buffer.write(open(os.environ['SCHNEGGI_ASKPASS_FILE'], 'rb').read())\n"
        )
        os.chmod(askpass, 0o700)
        env = os.environ.copy()
        env.update({
            "SSH_ASKPASS": str(askpass),
            "SSH_ASKPASS_REQUIRE": "force",
            "SCHNEGGI_ASKPASS_FILE": str(secret_file),
        })

        print("Copying the OTA ZIP to the Home Assistant host…", flush=True)
        copy = subprocess.run(
            ["scp", *ssh_options, str(args.archive), f"{args.host}:{remote_zip}"],
            stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            env=env,
            text=True,
        )
        if copy.returncode:
            sys.stdout.write(copy.stdout)
            raise RuntimeError("SCP failed; Home Assistant configuration was not changed")

        remote_script = build_remote_script()
        remote_command = (
            "sudo -k -S -p '' bash -c " + shlex.quote(remote_script) + " -- "
            + " ".join(shlex.quote(value) for value in (
                remote_zip, args.config_dir, ota_subdir.as_posix(), args.container
            ))
        )
        print("Checking the staged images, installing them, and restarting Home Assistant…", flush=True)
        result = subprocess.run(
            ["ssh", "-T", *ssh_options, args.host, remote_command],
            input=secret_file.read_bytes(),
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            env=env,
        )
        sys.stdout.buffer.write(result.stdout)
        sys.stdout.flush()
        if result.returncode:
            raise RuntimeError(
                f"Remote deployment failed (exit {result.returncode}); "
                f"the uploaded temporary ZIP may remain at {remote_zip}"
            )

    print("Deployment completed.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path, help="official Home Assistant OTA ZIP from a GitHub release")
    parser.add_argument("--host", required=True, help="SSH destination, for example admin@truenas")
    parser.add_argument("--config-dir", required=True, help="host path mounted as Home Assistant /config")
    parser.add_argument("--container", required=True, help="Home Assistant Docker container name")
    parser.add_argument("--ota-subdir", default="zigbee_ota", help="OTA directory under the host config directory")
    parser.add_argument(
        "--password-prompt", choices=("auto", "gui", "terminal"), default="auto",
        help="prompt once via Zenity when a desktop is available, otherwise use the terminal",
    )
    args = parser.parse_args()
    try:
        deploy(args)
    except (OSError, ValueError, RuntimeError, zipfile.BadZipFile, json.JSONDecodeError) as error:
        parser.exit(1, f"OTA deployment failed: {error}\n")


if __name__ == "__main__":
    main()
