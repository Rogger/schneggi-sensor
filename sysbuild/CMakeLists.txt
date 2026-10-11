# SPDX-License-Identifier: GPL-3.0-or-later
find_package(Sysbuild REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(sysbuild LANGUAGES)

sysbuild_get(app_config_dir IMAGE ${DEFAULT_IMAGE} VAR APPLICATION_BINARY_DIR CACHE)

# Run after the add-on creates its image. Keep the deployed Schneggi version
# encoding without modifying Nordic's SDK or Zigbee add-on sources.
add_custom_target(schneggi_ota_compat ALL
  COMMAND ${PYTHON_EXECUTABLE} ${APP_DIR}/scripts/ota_compat.py
    ${CMAKE_BINARY_DIR} ${app_config_dir}/zephyr/.config
  DEPENDS zigbee_ota_image
  VERBATIM
)
