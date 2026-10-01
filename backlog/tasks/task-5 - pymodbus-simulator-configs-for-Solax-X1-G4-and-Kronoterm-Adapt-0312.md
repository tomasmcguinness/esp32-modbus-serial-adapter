---
id: TASK-5
title: pymodbus simulator configs for Solax X1-G4 and Kronoterm Adapt 0312
status: Done
assignee: []
created_date: '2026-10-01 10:57'
updated_date: '2026-10-01 10:58'
labels:
  - tools
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The Matter examples in examples/matter/SolaxX1G4 and examples/matter/KronotermAdapt can only be tested against real hardware today. tools/pymodbus-sdm120-simulator already lets the SDM120M example be tested from a PC over a USB RS-485 adapter; add equivalent pymodbus simulator configs for the Solax X1-G4 inverter and the Kronoterm Adapt 0312 heat pump so those examples can be tested the same way.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 A Solax X1-G4 simulator config answers the example's input register poll with plausible values for every register it decodes
- [x] #2 A Kronoterm Adapt 0312 simulator config answers every holding register block the example polls with plausible values
- [x] #3 Both configs load in pymodbus.simulator and return the documented values when read by a Modbus client
- [x] #4 The simulator README documents the new configs, their register values and how to launch each
- [x] #5 The SolaxX1G4 and KronotermAdapt example READMEs point to the simulator
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Added two pymodbus simulator configs next to the existing SDM120M one in tools/pymodbus-sdm120-simulator:

- solax-x1-g4.json (server/device `solax`): input registers 0x0000-0x001C, covering the single 29-register block examples/matter/SolaxX1G4 polls. Registers the example does not decode are defined as 0 so the block read succeeds.
- kronoterm-adapt-0312.json (server/device `kronoterm`): holding registers for the eight blocks examples/matter/KronotermAdapt polls, at wire addresses (Kronoterm register number minus 1). Registers outside those blocks are undefined and return an exception.

Both use 9600 8N1 and the same COM28 port as config.json, matching the examples' menuconfig defaults.

The simulator README now covers all three devices: launch command per device, register/value tables, and how to enter negative S16 values as uint16. It also fixes the stale firmware/main/sdm120.cpp path. The SolaxX1G4 and KronotermAdapt READMEs gained a "Testing without ..." section linking to the simulator.

Verification: ran each config in pymodbus.simulator 3.15.0 with the server switched to TCP (no serial adapter available in this environment) and read back every block the firmware requests with a pymodbus client; values matched the README tables, and reads outside the defined registers returned exception 2. Not tested over RS-485 against the actual example firmware.
<!-- SECTION:FINAL_SUMMARY:END -->
