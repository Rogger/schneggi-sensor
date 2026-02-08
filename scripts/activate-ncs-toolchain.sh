#!/usr/bin/env bash
set -euo pipefail

# Activate the nRF Connect SDK toolchain environment for NCS v2.5.2.
# Source this script from your shell:
#   source scripts/activate-ncs-toolchain.sh

NCS_VERSION="${NCS_VERSION:-v2.5.2}"
TOOLCHAIN_PATH="${TOOLCHAIN_PATH:-$HOME/ncs/toolchains/7795df4459}"
NRFUTIL_SDK_MANAGER="${NRFUTIL_SDK_MANAGER:-$HOME/.vscode/extensions/nordic-semiconductor.nrf-connect-2026.1.1327-linux-x64/platform/nrfutil/bin/nrfutil-sdk-manager}"

if [[ ! -x "${NRFUTIL_SDK_MANAGER}" ]]; then
  echo "nrfutil-sdk-manager not found: ${NRFUTIL_SDK_MANAGER}" >&2
  echo "Set NRFUTIL_SDK_MANAGER to your installed extension path." >&2
  return 1 2>/dev/null || exit 1
fi

if [[ ! -d "${TOOLCHAIN_PATH}" ]]; then
  echo "Toolchain path not found: ${TOOLCHAIN_PATH}" >&2
  echo "Set TOOLCHAIN_PATH to your installed nRF toolchain directory." >&2
  return 1 2>/dev/null || exit 1
fi

if ! env_script="$("${NRFUTIL_SDK_MANAGER}" toolchain env --toolchain-path "${TOOLCHAIN_PATH}" --as-script sh 2>/dev/null)"; then
  # Fallback for restricted environments where HOME is not writable.
  env_script="$(HOME=/tmp "${NRFUTIL_SDK_MANAGER}" toolchain env --toolchain-path "${TOOLCHAIN_PATH}" --as-script sh)"
fi

set +u
eval "${env_script}"
set -u
export NCS_TOOLCHAIN_VERSION="NONE"
export NCS_VERSION

echo "Activated nRF toolchain from: ${TOOLCHAIN_PATH}"
echo "python: $(command -v python3)"
echo "west:   $(command -v west)"
