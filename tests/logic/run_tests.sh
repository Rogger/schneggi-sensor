#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD_DIR="${TMPDIR:-/tmp}/schneggi-sensor-tests"

mkdir -p "${BUILD_DIR}"

cc -std=c11 -Wall -Wextra -Werror \
  -I"${ROOT_DIR}/include" \
  "${ROOT_DIR}/src/sensor_logic.c" \
  "${ROOT_DIR}/tests/logic/test_sensor_logic.c" \
  -o "${BUILD_DIR}/test_sensor_logic"

"${BUILD_DIR}/test_sensor_logic"
