#pragma once

#define CHIP_DEVICE_CONFIG_DEVICE_VENDOR_NAME                    "tomasmcguinness.com"
#define CHIP_DEVICE_CONFIG_DEVICE_PRODUCT_NAME                   "Modbus Adapter"

// The software version is NOT set here. On ESP32, ConfigurationManagerImpl
// overrides both getters: GetSoftwareVersionString() returns the ESP-IDF app
// description version and GetSoftwareVersion() returns
// CHIP_CONFIG_SOFTWARE_VERSION_NUMBER, which esp_matter derives from
// PROJECT_VER_NUMBER. Both are set in the top-level CMakeLists.txt, so
// CHIP_DEVICE_CONFIG_DEVICE_SOFTWARE_VERSION* would be silently ignored here.

// Only the hardware version *string* belongs here. The numeric version, the
// vendor/product IDs and the device type come from sdkconfig, because
// src/platform/ESP32/CHIPDevicePlatformConfig.h nests those three defines
// inside a single "#ifndef CHIP_DEVICE_CONFIG_DEVICE_PRODUCT_ID" guard --
// defining the product ID here would silently suppress the other two.
#define CHIP_DEVICE_CONFIG_DEFAULT_DEVICE_HARDWARE_VERSION_STRING "Rev A"
