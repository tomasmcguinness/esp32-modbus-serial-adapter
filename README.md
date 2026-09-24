# Matter ESP32 Modbus Adapter
The goal of this project is to provide a simple working Modbus adapter. It is designed for the ESP32-C6 MCU, so can be used with ESPHome, ESP-IDF and Arduino.

> [!WARNING]
> This is a work in progress. The code is working without issue, but the PCB (Revision A) has some design issues, which need addressing.

# Hardware

The hardware folder contains a KiCad PCB design, designed around the ESP32-C6-MINI-1. Both the antenna and non-antenna versions will work here.

# SDM120M - Electrical Sensor

The first device supported by this project is the Eastron SDM120M Single Phase Energy Meter

The readings from this device will be exposed using as a Matter Electrical Sensor Device Type.

# Status LED

The board carries a single status LED (D2) on GPIO14. It reports what the device is doing:

| Pattern | Meaning |
| --- | --- |
| Slow blink (1s on, 1s off) | Ready to be paired — the commissioning window is open |
| Rapid blink (100ms on, 100ms off) | Pairing in progress |
| Dark, with a brief flash every 5s | Commissioned. Each flash is one Modbus poll of the meter |

The flash fires whether or not the meter answers, so it also shows the poll loop is alive on a device with nothing attached to the RS-485 bus. Read failures are reported in the log only.

# Matter Device Identity

The device advertises the following in the Basic Information cluster:

| Attribute | Value |
| --- | --- |
| Vendor name | tomasmcguinness.com |
| Product name | Modbus Energy Adapter |
| Vendor ID | 0xFFF1 (CSA test VID) |
| Product ID | 0x8000 |
| Software version | 1.0.0 (0x00010000) |
| Hardware version | 0 / "Rev A" |

The vendor and product names, plus the hardware version string, are set in
`firmware/main/CHIPProjectConfig.h`, which the build picks up through
`CONFIG_CHIP_PROJECT_CONFIG` in `sdkconfig.defaults`. Two things deliberately live
elsewhere:

- **Vendor ID, product ID and the numeric hardware version** stay in sdkconfig. Do not
  move them into the header: `src/platform/ESP32/CHIPDevicePlatformConfig.h` nests the
  product ID, device type and numeric hardware version behind a single `#ifndef`, so
  defining the product ID in the header silently drops the other two.
- **The software version** is set as `PROJECT_VER` / `PROJECT_VER_NUMBER` in
  `firmware/CMakeLists.txt`. The `CHIP_DEVICE_CONFIG_DEVICE_SOFTWARE_VERSION*` macros
  have no effect on ESP32 — `ConfigurationManagerImpl` overrides both getters and reads
  the ESP-IDF app description version and `CHIP_CONFIG_SOFTWARE_VERSION_NUMBER` instead.

When cutting a release, bump both `PROJECT_VER` and `PROJECT_VER_NUMBER`. The OTA
requestor compares the numeric value, so a new build with an unchanged number will not be
offered as an update. Leaving `PROJECT_VER_NUMBER` unset makes it 0.
