---
id: TASK-4
title: Matter Heat Pump example for Kronoterm Adapt 0312
status: In Progress
assignee: []
created_date: '2026-09-24 16:52'
updated_date: '2026-09-25 05:40'
labels:
  - firmware
  - matter
dependencies: []
references:
  - /home/tomasmcguinness/.claude/plans/like-the-sdm120m-and-jiggly-cook.md
  - >-
    https://github.com/kosl/kronoterm2mqtt/blob/main/kronoterm2mqtt/definitions/kronoterm_ksm.toml
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
New example examples/matter/KronotermAdapt, derived from the SolaxX1G4 example. Reads a Kronoterm Adapt 0312 heat pump over Modbus RTU (holding registers) and exposes it read-only as a Matter Heat Pump with a heating loop 1 Thermostat and outdoor/flow/return Temperature Sensor sub-parts.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Top-level endpoint is Heat Pump + Power Source (wired) + Electrical Sensor reporting active power
- [x] #2 Heating loop 1 is a Thermostat sub-part with local temperature, heating setpoint, system mode and running state
- [x] #3 Outdoor, flow and return temperatures are Temperature Sensor sub-parts with semantic tags
- [x] #4 Controller writes to thermostat attributes are rejected (read-only)
- [x] #5 Modbus reads use holding registers (FC 0x03); baud rate and slave address configurable via menuconfig (default 9600 / 1)
- [x] #6 Example builds with idf.py for esp32c6
- [x] #7 README documents endpoints, registers, wiring and known gaps
- [x] #8 Hot water tank is a Water Heater sub-part reporting tank temperature, target temperature, heat demand while the heat pump heats DHW, boost (fast DHW) and DHW mode; Boost/ChangeToMode are rejected
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented in examples/matter/KronotermAdapt. Builds clean with ESP-IDF 5.5.4 + esp_matter 1.4.2 (app 0x1ae3c0, 10% free in the OTA partition). EP1 built by hand (Heat Pump + Power Source wired + Electrical Sensor, EPM AC with ActivePower only, Node topology); heat_pump::create() not used because it forces DEM + EEM. EP2 thermostat (Heat+Cool features), EPs 3-5 temperature sensors, all parented to EP1 with tag lists (Common Number/Location/Direction; IDs checked against the Matter 1.4 namespace XML). The firmware's own updates use attribute::report(), which bypasses PRE_UPDATE, so the app callback rejects every PRE_UPDATE on the thermostat and controller writes fail. modbus.cpp generalised to read_registers(fc) with modbus_read_holding_registers(). Register map from kosl/kronoterm2mqtt (one-based, wire addr = reg - 1). Still needs testing against the real heat pump: register availability on the Adapt 0312, the 2160 room sensor sentinel, sign/scale of 2129, and chip-tool checks that the descriptor and thermostat writes are rejected.

Added EP6 Water Heater (2026-09-25). The Thermostat is heating only, with setpoint limits of 10-75 °C. main/water_heater_delegates.{h,cpp} has read-only delegates for Water Heater Management (HeaterTypes=HeatPump, HeatDemand from working function 1/4, BoostState from 2010) and Water Heater Mode (Off/On/Scheduled from 2026, updated via the delegate's mInstance->UpdateCurrentMode). New registers: 2010, 2023-2024, 2026, 2102. The first block is now 2000-2010. MAX_DYNAMIC_ENDPOINT_COUNT is 7. Builds clean (app 0x1b0730, 10% free). Still needs hardware checks that 2010/2023/2024/2026 exist on the Adapt 0312.
<!-- SECTION:NOTES:END -->
