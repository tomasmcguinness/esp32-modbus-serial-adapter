# Matter Electrical Sensor - Eastron SDM120M

This example turns the ESP32 Modbus Adapter into a Matter **Electrical Sensor** for an [Eastron SDM120M](https://www.eastroneurope.com/products/view/sdm120modbus) single phase energy meter. The adapter polls the meter over Modbus RTU and publishes the readings through the Matter Electrical Power Measurement cluster, so the meter can be added to any Matter controller (Apple Home, Google Home, Home Assistant, etc.).

It is built with [ESP-IDF](https://docs.espressif.com/projects/esp-idf/) and [esp-matter](https://github.com/espressif/esp-matter), and targets the ESP32-C6 on the adapter board.

## What it does

Every 5 seconds the adapter reads four input registers (function code `0x04`, two registers each, big-endian float32) from the meter at Modbus address `1`:

| Reading      | Register | Unit | Exposed over Matter as                 |
|--------------|----------|------|----------------------------------------|
| Voltage      | `0x0000` | V    | `Voltage` (mV)                         |
| Current      | `0x0006` | A    | `ActiveCurrent` (mA)                   |
| Active power | `0x000C` | W    | `ActivePower` (mW)                     |
| Total energy | `0x001A` | kWh  | Not yet exposed - logged and displayed |

The Matter node has a single Electrical Sensor endpoint with:

- **Power Topology** cluster, using the Node Topology feature.
- **Electrical Power Measurement** cluster, using the Alternating Current feature.

## Hardware

Designed for the Revision A adapter board (see [`hardware/`](../../../hardware)).

| Function            | GPIO |
|---------------------|------|
| RS-485 TX           | 22   |
| RS-485 RX           | 23   |
| RS-485 DE/RE        | 18   |
| Status LED (D2)     | 14   |

The serial link runs at **9600 baud, 8N1**, which matches the SDM120M's factory defaults. If you've changed the meter's baud rate or Modbus address, update `modbus.cpp` to match.

Wire the meter's A and B terminals to the adapter's A and B terminals.

### Status LED

| Pattern                                   | Meaning                                         |
|-------------------------------------------|-------------------------------------------------|
| Slow blink                                | Commissioning window open, ready to pair        |
| Rapid blink                               | Commissioning in progress                       |
| Off, with a brief flash every poll        | Commissioned; each flash is a Modbus read cycle |
| Off                                       | Not commissioned and commissioning window closed |

### Optional OLED display

An SSD1306 128x64 I2C OLED can show the current readings and the commissioning QR code. It is off by default; enable it with `idf.py menuconfig` → **Application Configuration** → **Enable status display (SSD1306 OLED)**.

> **Note:** the display driver uses GPIO22/23 for SDA/SCL, which clash with the RS-485 pins on the Revision A board. Only enable it on hardware where those pins are free, or change the pin assignments in `status_display.cpp`.

## Building and flashing

You need ESP-IDF installed and exported in your shell. The esp-matter component (`^1.4.0`) and LVGL are pulled in automatically by the component manager from `main/idf_component.yml`.

```sh
cd examples/matter/SDM120M
idf.py set-target esp32c6
idf.py build
idf.py -p <PORT> flash monitor
```

The first build takes a while because it compiles the Matter SDK.

## Commissioning

On first boot (or after all fabrics are removed) the device opens a commissioning window and advertises over BLE. The setup QR code is printed to the serial log:

```
I (xxxx) Main: Generated QR CODE [22]: MT:...
```

Paste that code into a QR code generator, or use the manual pairing code, and add the device from your Matter controller. The development build uses the test Vendor ID and default test passcode, so your controller may warn that the device is uncertified.

## Configuration

| What                                  | Where                                                   |
|---------------------------------------|---------------------------------------------------------|
| Vendor and product name               | `main/CHIPProjectConfig.h`                              |
| Hardware version string               | `main/CHIPProjectConfig.h`                              |
| Software version (string and number)  | `PROJECT_VER` / `PROJECT_VER_NUMBER` in `CMakeLists.txt` |
| Modbus pins, baud rate, slave address | `main/modbus.cpp`                                       |
| Registers read and poll interval      | `main/sdm120.cpp`                                       |
| Status LED pin and blink timings      | `main/status_led.cpp`                                   |

OTA updates are supported through the Matter OTA Requestor, using the two `ota_0`/`ota_1` partitions in `partitions.csv`. Remember to bump both `PROJECT_VER` and `PROJECT_VER_NUMBER` together when producing an update.

## Testing without a meter

[`tools/pymodbus-sdm120-simulator`](../../../tools/pymodbus-sdm120-simulator) simulates an SDM120M on a PC using a USB to RS-485 adapter, returning the same registers this example reads.

## Troubleshooting the RS-485 link

`main/main.cpp` has a `MODBUS_LINK_TEST` switch for bringing up the physical link independently of Modbus framing:

| Value | Test                                                                                    |
|-------|-----------------------------------------------------------------------------------------|
| `0`   | Normal operation - poll the SDM120M                                                     |
| `1`   | Transmit a single byte repeatedly (check with a scope or a USB RS-485 adapter)          |
| `2`   | Receive and log every byte that arrives                                                 |
| `3`   | Loopback - transmit a byte and listen for it. Jumper TXD to RXD to bypass the transceiver |

Every request and response is also logged as hex, so `idf.py monitor` is the first place to look if readings aren't arriving.