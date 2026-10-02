# Matter Solar Power - Solax X1 Hybrid G4

This example turns the ESP32 Modbus Adapter into a Matter **Solar Power** device for a [Solax X1 Hybrid G4](https://www.solaxpower.com/) single phase hybrid inverter. The adapter polls the inverter over Modbus RTU and publishes the AC output, both PV strings and the battery as separate sub-parts. Any Matter controller can then show them (Apple Home, Google Home, Home Assistant, etc.).

It is built with [ESP-IDF](https://docs.espressif.com/projects/esp-idf/) and [esp-matter](https://github.com/espressif/esp-matter), and targets the ESP32-C6 on the adapter board. It runs Matter over Thread on the ESP32-C6's 802.15.4 radio, so your Matter controller needs a Thread border router.

## Matter device layout

The inverter is the top-level endpoint and measures its own AC output. The PV strings and the battery are sub-parts (listed in the inverter's Descriptor `PartsList`).

| Endpoint | Part          | Device types                      | What it reports                                                 |
|----------|---------------|-----------------------------------|-----------------------------------------------------------------|
| 1        | Inverter      | Solar Power, Power Source (wired), Electrical Sensor (AC) | Power source status, AC output voltage, current and active power |
| 2        | PV string 1   | Electrical Sensor (DC)            | Voltage, current, active power                                  |
| 3        | PV string 2   | Electrical Sensor (DC)            | Voltage, current, active power                                  |
| 4        | Battery       | Electrical Sensor (DC), Power Source (battery) | Voltage, current, active power, state of charge, charge level, charging state |

The inverter endpoint is created with esp-matter's `solar_power::create()`. Its Electrical Sensor uses the Power Topology **Node** feature, which marks it as the measurement for the whole device. The sub-parts use the **Tree** feature, as each measures only itself. All four endpoints carry Descriptor semantic tags so controllers can tell them apart:

- **Namespaces:** Electrical Measurement AC/DC, Power Source Solar/Battery, and Common Number One/Two for the strings.
- **Labels:** "AC Output" (the inverter), "String 1", "String 2" and "Battery".

### Sign convention

Matter reports power flowing *into* an endpoint as positive:

- **The inverter's AC output and the PV strings** report **negative** power and current while generating.
- **Battery** is **positive while charging** and **negative while discharging**.

## What it reads

Every 5 seconds the adapter reads input registers `0x0000`–`0x001C` (function code `0x04`) in a single request:

| Register | Value              | Scale             | Reported on       |
|----------|--------------------|-------------------|-------------------|
| `0x0000` | Grid voltage       | 0.1 V             | EP1 Voltage       |
| `0x0001` | Grid current       | 0.1 A (signed)    | EP1 ActiveCurrent |
| `0x0002` | Inverter AC power  | 1 W (signed)      | EP1 ActivePower   |
| `0x0003` | PV1 voltage        | 0.1 V             | EP2 Voltage       |
| `0x0004` | PV2 voltage        | 0.1 V             | EP3 Voltage       |
| `0x0005` | PV1 current        | 0.1 A             | EP2 ActiveCurrent |
| `0x0006` | PV2 current        | 0.1 A             | EP3 ActiveCurrent |
| `0x000A` | PV1 power          | 1 W               | EP2 ActivePower   |
| `0x000B` | PV2 power          | 1 W               | EP3 ActivePower   |
| `0x0014` | Battery voltage    | 0.1 V (signed)    | EP4 Voltage, BatVoltage |
| `0x0015` | Battery current    | 0.1 A (signed)    | EP4 ActiveCurrent |
| `0x0016` | Battery power      | 1 W (signed)      | EP4 ActivePower   |
| `0x001C` | Battery SoC        | 1 %               | EP4 BatPercentRemaining |

The register map follows the Solax Hybrid X1/X3-G4 Modbus RTU protocol. The addresses and scales are all `#define`s at the top of `main/solax.cpp`, so they're easy to adjust if your firmware version differs.

## Hardware

Designed for the Revision A adapter board (see [`hardware/`](../../../hardware)).

| Function            | GPIO |
|---------------------|------|
| RS-485 TX           | 22   |
| RS-485 RX           | 23   |
| RS-485 DE/RE        | 18   |
| Status LED (D2)     | 14   |

Connect the adapter's A and B terminals to the RS-485 A and B pins on the inverter's COM/RS485 port. The pinout is in the Solax installation manual for your model.

### Inverter settings

On the inverter, go to **Settings → Advanced Settings → Modbus** (the path varies slightly between firmware versions) and set:

- **Baud rate:** 9600
- **Modbus address:** 1

Both defaults can be changed on the adapter side with `idf.py menuconfig` → **Application Configuration**.

### Status LED

| Pattern                                   | Meaning                                          |
|-------------------------------------------|--------------------------------------------------|
| Slow blink                                | Commissioning window open, ready to pair         |
| Rapid blink                               | Commissioning in progress                        |
| Off, with a brief flash every poll        | Commissioned; each flash is a Modbus read cycle  |
| Off                                       | Not commissioned and commissioning window closed |

## Building and flashing

You need ESP-IDF installed and exported in your shell. The esp-matter component (pinned to `~1.4.2`, the same release as the SDM120M example) is pulled in automatically by the component manager.

```sh
cd examples/matter/SolaxX1G4
idf.py set-target esp32c6
idf.py build
idf.py -p <PORT> flash monitor
```

The first build takes a while because it compiles the Matter SDK.

## Commissioning

On first boot (or after all fabrics are removed) the device opens a commissioning window and advertises over BLE. The controller uses BLE to give it the Thread network credentials, after which it joins the Thread network as a router. The setup QR code is printed to the serial log:

```
I (xxxx) Main: Generated QR CODE [22]: MT:...
```

Add the device from your Matter controller using that code. The development build uses the test Vendor ID and default test passcode, so your controller may warn that the device is uncertified.

## Configuration

| What                                  | Where                                                    |
|---------------------------------------|----------------------------------------------------------|
| Modbus baud rate and slave address    | `idf.py menuconfig` → Application Configuration          |
| Vendor and product name               | `main/CHIPProjectConfig.h`                               |
| Software version (string and number)  | `PROJECT_VER` / `PROJECT_VER_NUMBER` in `CMakeLists.txt` |
| Modbus pins                           | `main/modbus.cpp`                                        |
| Registers, scaling and poll interval  | `main/solax.cpp`                                         |
| Measurement ranges reported to Matter | `main/electrical_power_measurement_delegate.cpp`         |
| Battery warning/critical thresholds   | `main/main.cpp`                                          |

OTA updates are supported through the Matter OTA Requestor. Remember to bump both `PROJECT_VER` and `PROJECT_VER_NUMBER` together when producing an update.

## Testing without an inverter

[`tools/pymodbus-sdm120-simulator`](../../../tools/pymodbus-sdm120-simulator) simulates the inverter on a PC using a USB to RS-485 adapter, using its `solax-x1-g4.json` config to return the same registers this example reads.

## Troubleshooting the RS-485 link

`main/main.cpp` has a `MODBUS_LINK_TEST` switch for bringing up the physical link independently of Modbus framing:

| Value | Test                                                                                      |
|-------|-------------------------------------------------------------------------------------------|
| `0`   | Normal operation - poll the inverter                                                      |
| `1`   | Transmit a single byte repeatedly (check with a scope or a USB RS-485 adapter)            |
| `2`   | Receive and log every byte that arrives                                                   |
| `3`   | Loopback - transmit a byte and listen for it. Jumper TXD to RXD to bypass the transceiver |

Every request and response is logged as hex, and each poll logs the decoded readings. Compare these with the inverter's display or the Solax app to check the scaling and signs.

## Known limitations

- **No energy (kWh) reporting.** The inverter endpoint has the Electrical Energy Measurement cluster the Solar Power device type requires, but the adapter doesn't read the inverter's energy counters yet, so cumulative exported energy is reported as null.
- **The battery is not a Battery Storage device.** It's modelled as an Electrical Sensor with a Power Source (battery feature). The full Battery Storage device type needs Device Energy Management, which doesn't make sense for a read-only adapter.
- **Read only.** The inverter can't be controlled from Matter.
