add_executable(test_ota test_ota.c)
target_include_directories(test_ota PRIVATE stubs ../../../include)
add_test(NAME ota COMMAND test_ota)

add_executable(test_ota_usb test_ota.c)
target_compile_definitions(test_ota_usb PRIVATE OTA_USB_PROFILE=1)
target_include_directories(test_ota_usb PRIVATE stubs ../../../include)
add_test(NAME ota_usb COMMAND test_ota_usb)
