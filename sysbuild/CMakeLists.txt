# SPDX-License-Identifier: GPL-3.0-or-later
find_package(Sysbuild REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(sysbuild LANGUAGES)

sysbuild_get(app_config_dir IMAGE ${DEFAULT_IMAGE} VAR APPLICATION_BINARY_DIR CACHE)

# Use the add-on's header writer with the full Schneggi version. Its default
# target passes through a CLI limited to 8-bit patch and build components.
set_property(TARGET zigbee_ota_image PROPERTY EXCLUDE_FROM_ALL TRUE)
add_custom_target(schneggi_ota_image ALL
  COMMAND ${PYTHON_EXECUTABLE} -B "${APP_DIR}/scripts/ota_image.py" image
    "${CMAKE_BINARY_DIR}" "${app_config_dir}/zephyr/.config"
    "${ZEPHYR_NCS_ZIGBEE_MODULE_DIR}/resources/zb_add_ota_header.py"
  DEPENDS "${CMAKE_BINARY_DIR}/dfu_multi_image.bin"
  VERBATIM
)
