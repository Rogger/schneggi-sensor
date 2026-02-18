#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="${TMPDIR:-/tmp}/schneggi-zigbee-signal-tests"

mkdir -p "${BUILD_DIR}"

CC_BIN="${CC:-}"
if [[ -z "${CC_BIN}" ]]; then
	for candidate in gcc clang cc; do
		if command -v "${candidate}" >/dev/null 2>&1; then
			CC_BIN="${candidate}"
			break
		fi
	done
fi

if [[ -z "${CC_BIN}" ]]; then
	echo "No suitable C compiler found (tried: \$CC, gcc, clang, cc)" >&2
	exit 127
fi

"${CC_BIN}" -std=c11 -Wall -Wextra -Werror \
	-I"${ROOT_DIR}/include" \
	"${ROOT_DIR}/src/zigbee_signal_logic.c" \
	"${ROOT_DIR}/tests/unit/zigbee_signal_logic/test_signal_logic.c" \
	-o "${BUILD_DIR}/test_zigbee_signal_logic"

"${BUILD_DIR}/test_zigbee_signal_logic"
