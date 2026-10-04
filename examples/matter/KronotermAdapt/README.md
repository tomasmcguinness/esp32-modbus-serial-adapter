# Matter Heat Pump - Kronoterm Adapt 0312

This example turns the ESP32 Modbus Adapter into a Matter **Heat Pump** device for a [Kronoterm Adapt](https://kronoterm.com/) 0312 air-to-water heat pump. The adapter polls the heat pump controller over Modbus RTU and publishes these readings:

- the power the heat pump draws
- heating loop 1, as a thermostat
- the outdoor, flow and return temperatures
- the hot water tank, including whether the heat pump is heating it right now

Any Matter controller can then show them (Apple Home, Google Home, Home Assistant, etc.).

It is built with [ESP-IDF](https://docs.espressif.com/projects/esp-idf/) and [esp-matter](https://github.com/espressif/esp-matter), and targets the ESP32-C6 on the adapter board. It runs Matter over Thread on the ESP32-C6's 802.15.4 radio, so your Matter controller needs a Thread border router.

## Matter device layout

The heat pump is the top-level endpoint. Each thing it measures is a sub-part (listed in the heat pump's Descriptor `PartsList`).

| Endpoint | Part           | Device types                                         | What it reports                                                        |
|----------|----------------|------------------------------------------------------|------------------------------------------------------------------------|
| 1        | Heat pump      | Heat Pump, Power Source (wired AC), Electrical Sensor | Active power drawn by the heat pump                                    |
| 2        | Heating loop 1 | Thermostat (heating and cooling)                     | Room temperature, heating setpoint, system mode, running state         |
| 3        | Outdoor        | Temperature Sensor                                   | Outdoor temperature                                                    |
| 4        | Flow           | Temperature Sensor                                   | Heat pump outlet (flow) temperature                                    |
| 5        | Return         | Temperature Sensor                                   | Heat pump inlet (return) temperature                                   |
| 6        | Hot water      | Water Heater                                         | Tank temperature, target temperature, heating now, fast heating, DHW mode |

The Electrical Sensor uses the Power Topology **Node** feature, because it measures the whole heat pump. The sub-parts carry Descriptor semantic tags so controllers can tell the three temperature sensors apart:

- **Loop 1:** Common Number One, labelled "Loop 1".
- **Outdoor:** Common Location Outdoor, labelled "Outdoor".
- **Flow / Return:** Common Direction Forward / Backward, labelled "Flow" and "Return". Matter has no flow/return namespace, so the labels carry the meaning.

### Thermostat mapping

| Thermostat attribute      | Source                                                                                                  |
|---------------------------|---------------------------------------------------------------------------------------------------------|
| `LocalTemperature`        | Loop 1 room thermostat temperature. **Null** when no room thermostat is fitted (the controller reports -40 °C). |
| `OccupiedHeatingSetpoint` | Loop 1 current desired room temperature, including ECO/comfort offsets                                  |
| `SystemMode`              | Off if the system or loop 1 is switched off. Otherwise Heat, Cool or Off from the operation regime     |
| `ThermostatRunningState`  | Heat (or Cool) while the heat pump is heating (or cooling) and the loop 1 circulation pump is running   |

### Hot water mapping

The Water Heater device type is a heating-only Thermostat plus the Water Heater Management and Water Heater Mode clusters.

| Attribute                                   | Source                                                                                         |
|---------------------------------------------|------------------------------------------------------------------------------------------------|
| Thermostat `LocalTemperature`               | DHW tank temperature                                                                           |
| Thermostat `OccupiedHeatingSetpoint`        | Current desired DHW temperature, including ECO/comfort offsets. The limits go up to 75 °C.     |
| Thermostat `SystemMode`                     | Heat if the system is on and DHW operation isn't Off, otherwise Off                            |
| Thermostat `ThermostatRunningState`         | Heat while the heat pump is heating the tank (working function DHW or thermal disinfection)    |
| Water Heater Management `HeatDemand`        | Heat Pump while the heat pump is heating the tank, otherwise empty                             |
| Water Heater Management `BoostState`        | Active while fast DHW heating is on                                                            |
| Water Heater Mode `CurrentMode`             | DHW operation: Off, On (Manual tag) or Scheduled (Timed tag)                                   |

`HeaterTypes` reports Heat Pump only.

Both thermostats are **read-only**. Writes from a controller (changing the setpoint or mode) are rejected. So are the Boost, CancelBoost and ChangeToMode commands. Matter never shows a value the heat pump didn't receive.

## What it reads

Every 10 seconds the adapter reads these holding registers (function code `0x03`) in eight small requests:

| Register | Value                                  | Scale  | Reported on                       |
|----------|----------------------------------------|--------|-----------------------------------|
| 2000     | System operation (0 off, 1 on)         | -      | EP2 SystemMode                    |
| 2001     | Working function (0 heating, 1 DHW, 2 cooling, 4 thermal disinfection, ...) | - | EP2 and EP6 ThermostatRunningState, EP6 HeatDemand |
| 2006     | Error/warning status                   | -      | Log only                          |
| 2007     | Operation regime (0 cooling, 1 heating, 2 off) | - | EP2 SystemMode                  |
| 2010     | Fast DHW heating                       | -      | EP6 BoostState                    |
| 2023     | Desired DHW temperature                | 0.1 °C | Log only                          |
| 2024     | Current desired DHW temperature        | 0.1 °C | EP6 OccupiedHeatingSetpoint       |
| 2026     | DHW operation (0 off, 1 on, 2 scheduled) | -    | EP6 CurrentMode, SystemMode       |
| 2042     | Loop 1 operation (0 off, 1 on, 2 scheduled) | -  | EP2 SystemMode                    |
| 2045     | Loop 1 circulation pump                | -      | EP2 ThermostatRunningState        |
| 2101     | HP inlet (return) temperature          | 0.1 °C | EP5 MeasuredValue                 |
| 2102     | DHW tank temperature                   | 0.1 °C | EP6 LocalTemperature              |
| 2103     | Outdoor temperature                    | 0.1 °C | EP3 MeasuredValue                 |
| 2104     | HP outlet (flow) temperature           | 0.1 °C | EP4 MeasuredValue                 |
| 2129     | Current power consumption              | 1 W    | EP1 ActivePower                   |
| 2130     | Loop 1 flow temperature                | 0.1 °C | Log only                          |
| 2160     | Loop 1 room thermostat temperature     | 0.1 °C | EP2 LocalTemperature              |
| 2191     | Loop 1 current desired room temperature | 0.1 °C | EP2 OccupiedHeatingSetpoint      |

All values are signed 16-bit. Kronoterm numbers its registers from 1, so the address sent on the wire is one less (register 2101 is requested as address 2100).

The register map comes from the community [kronoterm2mqtt](https://github.com/kosl/kronoterm2mqtt) project, which lists ADAPT as supported. The addresses and scales are all `#define`s at the top of `main/kronoterm.cpp`, so they're easy to adjust if your controller firmware differs. Registers from 2126 onwards only exist on newer controllers with the extended Modbus map ([details](https://github.com/Favio25/Kronoterm-homeassistant/issues/39)).

## Hardware

Designed for the Revision A adapter board (see [`hardware/`](../../../hardware)).

| Function            | GPIO |
|---------------------|------|
| RS-485 TX           | 22   |
| RS-485 RX           | 23   |
| RS-485 DE/RE        | 18   |
| Status LED (D2)     | 14   |

Connect the adapter's A and B terminals to the RS-485 A and B terminals of the heat pump controller's Modbus interface. On some ADAPT units Modbus is only available through Kronoterm's optional Modbus/TEX interface module. The Kronoterm installation manual for your unit shows where it connects.

### Heat pump settings

The adapter defaults match the KSM controller's factory Modbus settings, so nothing needs changing on the heat pump:

- **Baud rate:** 115200
- **Modbus address:** 20

These come from Kronoterm's [BMS system manual](https://nau-gmbh.ch/wp-content/uploads/2025/03/Installation-and-Operating-Manual-for-BMS-System-Datenpunktliste.pdf) for the KSM controller. If your controller has been reconfigured (19200 baud is the other supported rate), change the adapter side to match with `idf.py menuconfig` → **Application Configuration**. The link is 8N1.

### Status LED

| Pattern                                   | Meaning                                          |
|-------------------------------------------|--------------------------------------------------|
| Slow blink                                | Commissioning window open, ready to pair         |
| Rapid blink                               | Commissioning in progress                        |
| Off, with a brief flash every poll        | Commissioned; each flash is a Modbus read cycle  |
| Off                                       | Not commissioned and commissioning window closed |

## Building and flashing

You need ESP-IDF installed and exported in your shell. The component manager pulls in esp-matter automatically. It's pinned to `~1.4.2`, the same release as the other examples.

```sh
cd examples/matter/KronotermAdapt
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
| Registers, scaling and poll interval  | `main/kronoterm.cpp`                                     |
| Power range reported to Matter        | `main/electrical_power_measurement_delegate.cpp`         |

OTA updates are supported through the Matter OTA Requestor. Remember to bump both `PROJECT_VER` and `PROJECT_VER_NUMBER` together when producing an update.

## Testing without a heat pump

[`tools/pymodbus-sdm120-simulator`](../../../tools/pymodbus-sdm120-simulator) simulates the heat pump on a PC using a USB to RS-485 adapter, using its `kronoterm-adapt-0312.json` config to return the same registers this example reads.

## Troubleshooting the RS-485 link

`main/main.cpp` has a `MODBUS_LINK_TEST` switch for bringing up the physical link independently of Modbus framing:

| Value | Test                                                                                      |
|-------|-------------------------------------------------------------------------------------------|
| `0`   | Normal operation - poll the heat pump                                                     |
| `1`   | Transmit a single byte repeatedly (check with a scope or a USB RS-485 adapter)            |
| `2`   | Receive and log every byte that arrives                                                   |
| `3`   | Loopback - transmit a byte and listen for it. Jumper TXD to RXD to bypass the transceiver |

Every request and response is logged as hex, and each poll logs the decoded readings. Compare these with the heat pump's display or the Kronoterm cloud app to check the scaling. If one block returns a Modbus exception, the controller probably doesn't support that register. Remove it from the `blocks` table in `main/kronoterm.cpp`.

## Known limitations

- **Read only.** The heat pump can't be controlled from Matter. Setpoint and mode writes are rejected.
- **No Device Energy Management or energy (kWh) reporting.** The Heat Pump device type expects both. DEM needs the device to act on power adjustment requests, which a read-only adapter can't do. The Modbus map has no cumulative energy register.
- **No voltage or current.** Only active power is available over Modbus.
- **Loop 1 and hot water only.** The buffer tank, pool and loops 2–4 aren't exposed yet.
- **No tank volume or tank percentage.** The hot water tank doesn't use the Water Heater Management Energy Management or Tank Percent features, because the Modbus map has nothing to back them.
- **Community register map.** It hasn't been checked against an official Kronoterm document for the Adapt 0312. Some models report -40 °C for the loop thermostat registers, which is why LocalTemperature falls back to null.
