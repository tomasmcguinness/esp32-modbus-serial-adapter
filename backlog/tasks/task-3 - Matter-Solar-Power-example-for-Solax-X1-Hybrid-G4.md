---
id: TASK-3
title: Matter Solar Power example for Solax X1 Hybrid G4
status: In Progress
assignee: []
created_date: '2026-09-24 06:33'
updated_date: '2026-09-24 06:45'
labels:
  - firmware
  - matter
dependencies: []
references:
  - >-
    /home/tomasmcguinness/.claude/plans/using-the-sdm120m-example-graceful-key.md
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
New example examples/matter/SolaxX1G4, derived from the SDM120M example. Reads the Solax X1 Hybrid G4 over Modbus RTU and exposes it as a Matter Solar Power device, with AC power, PV string 1, PV string 2 and battery as sub-part endpoints.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Top-level endpoint is Solar Power + Power Source (wired) with sub-parts for AC output, PV1, PV2 and battery
- [x] #2 PV strings and battery report DC Electrical Power Measurement; AC output reports AC
- [x] #3 Battery endpoint exposes Power Source battery voltage and percent remaining
- [x] #4 Modbus baud rate and slave address configurable via menuconfig (default 9600 / 1)
- [x] #5 Example builds with idf.py for esp32c6
- [x] #6 README documents endpoints, registers, wiring and known gaps
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented in examples/matter/SolaxX1G4. Builds clean with ESP-IDF 5.5.4 + esp_matter 1.4.2 (pinned ~1.4.2; unpinned ^1.4.0 resolved to 1.6.0, whose API differs). Endpoints are built by hand because solar_power::create puts the electrical sensor on the inverter endpoint and electrical_sensor::add forces NODE topology. Sub-parts use TREE topology and semantic tags (namespaces 0x0A/0x0F/0x07). Still needs testing against a real inverter: register map, scaling and signs, and chip-tool reads of the descriptor/EPM/power source.
<!-- SECTION:NOTES:END -->
