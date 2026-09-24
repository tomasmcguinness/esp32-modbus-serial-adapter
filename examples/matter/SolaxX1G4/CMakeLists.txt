# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Matter software version. PROJECT_VER becomes the ESP-IDF app description
# version, which ESP32's ConfigurationManagerImpl reports as the Basic
# Information SoftwareVersionString. PROJECT_VER_NUMBER is picked up by
# esp_matter as CHIP_CONFIG_SOFTWARE_VERSION_NUMBER, reported as
# SoftwareVersion and used by the OTA requestor to compare builds; it defaults
# to 0 if left unset. Bump both together. Must be set before project().
set(PROJECT_VER "1.0.0")
set(PROJECT_VER_NUMBER 0x00010000)

project(matter-esp32-modbus-solax-x1g4)

idf_build_set_property(CXX_COMPILE_OPTIONS "-std=gnu++17;-Os;-DCHIP_HAVE_CONFIG_H;-Wno-overloaded-virtual" APPEND)
idf_build_set_property(C_COMPILE_OPTIONS "-Os" APPEND)
idf_build_set_property(COMPILE_OPTIONS "-Wno-format-nonliteral;-Wno-format-security" APPEND)
