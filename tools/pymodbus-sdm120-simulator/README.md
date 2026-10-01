# pymodbus-sdm120-simulator

Simulates the Modbus RTU devices the Matter examples talk to, so the adapter firmware
can be tested against a PC instead of (or alongside) the real device or the
`tools/modbus-slave-simulator` hardware simulator.

There is one config file per device:

| Device                    | Config file                 | Server/device name | Example                                                          |
|---------------------------|-----------------------------|--------------------|------------------------------------------------------------------|
| Eastron SDM120M meter     | `config.json`               | `sdm120`           | [`examples/matter/SDM120M`](../../examples/matter/SDM120M)       |
| Solax X1 Hybrid G4        | `solax-x1-g4.json`          | `solax`            | [`examples/matter/SolaxX1G4`](../../examples/matter/SolaxX1G4)   |
| Kronoterm Adapt 0312      | `kronoterm-adapt-0312.json` | `kronoterm`        | [`examples/matter/KronotermAdapt`](../../examples/matter/KronotermAdapt) |

All three listen at 9600 8N1 and answer on any slave address, which matches the
examples' defaults (`idf.py menuconfig` → Application Configuration).

## Hardware

PC → YP-05 (USB↔TTL) → HW-519 (TTL↔RS485, auto direction) → A/B → adapter's RS485 bus.

The HW-519 handles TX/RX direction switching itself, so no RTS/DE toggling is needed
on the PC side.

Only one Modbus RTU slave can be on the bus at a time — unplug/power down the
`modbus-slave-simulator` board (or the real device) before starting this simulator.

## Setup

First, create a venv

```
python3 -m venv ./venv
```

and then start it.

```
source venv/bin/activate
```

Once it has started, install the required dependencies

```
pip install pymodbus[serial] aiohttp
```

(Already satisfied if you've installed `pymodbus`, `pyserial`, and `aiohttp` for
other tools in this repo.)

Find the YP-05's COM port in Device Manager (it enumerates as a CH340 USB Serial
device — distinct from the ESP32 boards, which show up as `USB\VID_303A...`).
Edit `"port"` in the config file you're using to match, e.g. `"COM5"`.

## Usage

Run from this directory, picking the line for the device you want to simulate:

```
pymodbus.simulator --modbus_server sdm120 --modbus_device sdm120 --json_file sdm120m.json
pymodbus.simulator --modbus_server solax --modbus_device solax --json_file solax-x1-g4.json
pymodbus.simulator --modbus_server kronoterm --modbus_device kronoterm --json_file kronoterm-adapt-0312.json
```

(`pymodbus.simulator` is the console script `pip` installs alongside the package —
run `where pymodbus.simulator` if it's not on your PATH.)

This starts the RTU server on the configured COM port, plus a web console at
`http://localhost:8080` where you can watch requests/responses live and edit
register values without restarting. You'll see two deprecation warnings
(`ModbusSimulatorContext`/`ModbusServerContext` "will be removed in v4") on
startup — harmless on the current 3.x releases, just pymodbus flagging that the
config format changes in 4.0.

Then run the matching example firmware (or point `tools/uart-send-byte`/a Modbus
master at the same bus) and it should read back the values below.

## Simulated registers

In every config the `addr` is the address sent on the wire.

### SDM120M (`sdm12m.json`)

Matches the registers `examples/matter/SDM120M/main/sdm120.cpp` reads (function
code 0x04, big-endian float32, no word swap):

| Register | Address | Value     |
|----------|---------|-----------|
| Voltage  | 0x0000  | 230.5 V   |
| Current  | 0x0006  | 1.23 A    |
| Power    | 0x000C  | 283.2 W   |
| Energy   | 0x001A  | 12.34 kWh |

Edit the `float32` values to test different readings.

### Solax X1-G4 (`solax-x1-g4.json`)

Matches the single block `examples/matter/SolaxX1G4/main/solax.cpp` reads
(function code 0x04, 29 registers from 0x0000). The registers in between that
the example doesn't decode are defined as 0 so the block read succeeds.

| Register        | Address | Raw  | Value   |
|-----------------|---------|------|---------|
| Grid voltage    | 0x0000  | 2401 | 240.1 V |
| Grid current    | 0x0001  | 62   | 6.2 A   |
| Grid power      | 0x0002  | 1480 | 1480 W  |
| PV1 voltage     | 0x0003  | 3205 | 320.5 V |
| PV2 voltage     | 0x0004  | 2980 | 298.0 V |
| PV1 current     | 0x0005  | 31   | 3.1 A   |
| PV2 current     | 0x0006  | 24   | 2.4 A   |
| PV1 power       | 0x000A  | 990  | 990 W   |
| PV2 power       | 0x000B  | 715  | 715 W   |
| Battery voltage | 0x0014  | 1024 | 102.4 V |
| Battery current | 0x0015  | 20   | 2.0 A   |
| Battery power   | 0x0016  | 205  | 205 W   |
| Battery SOC     | 0x001C  | 67   | 67 %    |

Battery current and power are positive when charging. To simulate discharging
(or grid import), see [Negative values](#negative-values).

### Kronoterm Adapt 0312 (`kronoterm-adapt-0312.json`)

Matches the blocks `examples/matter/KronotermAdapt/main/kronoterm.cpp` reads
(function code 0x03). Kronoterm numbers its registers from 1, so the `addr` in
the config is one less than the register number. The simulated heat pump is on
and heating loop 1, with DHW enabled and no faults.

| Register                       | Number | `addr` | Raw | Value       |
|--------------------------------|--------|--------|-----|-------------|
| System operation               | 2000   | 1999   | 1   | On          |
| Working function               | 2001   | 2000   | 0   | Heating     |
| Error status                   | 2006   | 2005   | 0   | None        |
| Operation regime               | 2007   | 2006   | 1   | Heating     |
| Fast DHW heating               | 2010   | 2009   | 0   | Off         |
| DHW setpoint                   | 2023   | 2022   | 480 | 48.0 °C     |
| DHW current setpoint           | 2024   | 2023   | 480 | 48.0 °C     |
| DHW operation                  | 2026   | 2025   | 1   | On          |
| Loop 1 operation               | 2042   | 2041   | 1   | On          |
| Loop 1 pump                    | 2045   | 2044   | 1   | On          |
| Return temperature (HP inlet)  | 2101   | 2100   | 302 | 30.2 °C     |
| DHW tank temperature           | 2102   | 2101   | 465 | 46.5 °C     |
| Outdoor temperature            | 2103   | 2102   | 75  | 7.5 °C      |
| Flow temperature (HP outlet)   | 2104   | 2103   | 354 | 35.4 °C     |
| Power consumption              | 2129   | 2128   | 850 | 850 W       |
| Loop 1 flow temperature        | 2130   | 2129   | 348 | 34.8 °C     |
| Loop 1 room temperature        | 2160   | 2159   | 213 | 21.3 °C     |
| Loop 1 room setpoint           | 2191   | 2190   | 210 | 21.0 °C     |

The unused registers inside each block (2002–2005, 2008–2009, 2043–2044) are
defined as 0. Everything else is undefined, so a read that strays outside the
polled blocks returns an exception.

Useful values to try:

- Working function `1` (DHW), `2` (cooling) or `4` (thermal disinfection).
- Operation regime `0` (cooling) or `2` (off).
- Error status `1` (warning), `2` (error) or `3` (notification).
- Loop 1 room temperature `65136` (-40.0 °C), which is what the controller
  reports when no room thermostat is fitted.

### Negative values

The Solax and Kronoterm registers are signed 16-bit, but the simulator stores
them as `uint16`. Enter a negative reading as `65536 + raw`, e.g. an outdoor
temperature of -5.0 °C is raw -50, so `65486`; a battery discharging at 500 W
is `65036`.

## Notes

- `"type exception": true` means any register the firmware queries that isn't
  defined in the config returns a Modbus exception, same as a real device
  would for an unsupported register.
- Registers are read-only (`"write": []`); none of the examples write to the
  device.

