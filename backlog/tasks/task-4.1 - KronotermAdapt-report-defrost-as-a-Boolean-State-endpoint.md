---
id: TASK-4.1
title: 'KronotermAdapt: report defrost as a Boolean State endpoint'
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-09 07:18'
labels:
  - firmware
  - matter
dependencies: []
references:
  - >-
    https://github.com/kosl/kronoterm2mqtt/blob/main/kronoterm2mqtt/definitions/kronoterm_ksm.toml
  - examples/matter/KronotermAdapt/README.md
parent_task_id: TASK-4
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The Kronoterm controller reports whether the heat pump is defrosting (de-icing its outdoor evaporator) in holding register 2011, "Defrosting", a 0/1 binary per the community kronoterm_ksm.toml map. The KronotermAdapt example does not read it today; it stops at register 2010 (Fast DHW).

Defrost is independent of the working function (register 2001): it can happen while heating or while making hot water, so the controller can report "DHW" and "defrosting" at once. It must therefore be exposed as its own flag, not folded into the loop 1 ThermostatRunningState or the water heater HeatDemand, which keep reporting the working function unchanged. No standard Matter attribute carries defrost, so expose it as a Boolean State sub-part of the heat pump, identified by a Descriptor TagList label (as the temperature sensors are), StateValue true while defrosting.

The heating monitor (separate repo, matter-esp32-heating-monitor) will read this endpoint to record defrost time and the electricity it uses, so the endpoint and its label need to be stable and documented.

Register 2011 has not yet been confirmed on a real ADAPT 0312: the map is written for the KSM controller. Do not touch register 2019 (a "Defrosting" switch, likely a manual trigger); the adapter stays read-only.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Register 2011 is read every poll alongside the existing registers and logged
- [x] #2 A Boolean State sub-part of the heat pump reports StateValue true while register 2011 is 1 and false while it is 0
- [x] #3 The defrost endpoint is identified by a Descriptor TagList label that a controller can find without knowing the endpoint number
- [x] #4 Loop 1 ThermostatRunningState and water heater HeatDemand behave exactly as before during a defrost
- [x] #5 A failed read of register 2011 leaves the last reported value rather than reporting false
- [x] #6 README register table and endpoint list include register 2011 and the defrost endpoint
- [x] #7 pymodbus simulator config (TASK-5) includes register 2011 so the endpoint can be tested without the heat pump
- [x] #8 Verified against the real heat pump during an observed defrost, or the README states it is unverified
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. kronoterm.cpp: read register 2011 every poll by extending the first block to 2000-2011 (the user's call, 2026-11-13: 2011 is documented in Kronoterm's BMS manual, so it is safe to read with the block). Log the value each poll.
2. kronoterm.h: add defrosting to kronoterm_reading_t.
3. main.cpp: a Contact Sensor sub-part (the standard device type carrying Boolean State) parented to the heat pump. Its TagList is a manufacturer tag (mfgCode = the vendor ID) labelled "Defrost", the label the heating monitor matches. apply_readings reports StateValue only when the read succeeded, so a failed read leaves the last value. Loop 1 and hot water mapping are untouched.
4. README: endpoint 7, tag list, register table, request count, and the note that it is unverified on a real ADAPT 0312.
5. Simulator: kronoterm-adapt-0312.json gets addr 2010 (register 2011), and its README table too.
6. Build for esp32c6.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-13 (later): On the user's instruction, 2011 is now read as part of the first block (2000-2011, 12 registers, exactly the block buffer), not in its own request, because Kronoterm's BMS manual documents it. defrost_valid and read_defrost() are removed. A failed poll keeps every last value, defrost included, as before (AC #5). README and simulator README updated to match (eight requests). Rebuilt for esp32c6 (0x1996f0). The note below describes the earlier separate-request version.

2026-11-13: Implemented (uncommitted).
- kronoterm.cpp reads 2011 every poll in its own request (read_defrost()), so a controller that rejects it loses only defrost. A failed read sets defrost_valid false and logs a warning. "Defrost: yes/no/unknown" is logged every poll.
- main.cpp adds endpoint 7, a Contact Sensor (Boolean State) sub-part of the heat pump, created last so endpoints 1-6 keep their numbers. Its TagList is a manufacturer tag (mfgCode = CONFIG_DEVICE_VENDOR_ID, namespace 0, tag 0) labelled "Defrost". StateValue is reported only after a successful read, so a failed read keeps the last value (false from boot).
- Loop 1 ThermostatRunningState and water heater HeatDemand code is unchanged: AC #4 holds by construction.
- README: endpoint table, tag list, a Defrost mapping section, the register table (nine requests now), and a known limitation that 2011 is unverified on an ADAPT 0312 (AC #8 is satisfied that way).
- Simulator: addr 2010 added to kronoterm-adapt-0312.json, and its README table, with how to simulate a defrost and a rejected 2011.
- Builds for esp32c6 (0x1997a0, 15% free).
- Not run on hardware or against the simulator yet. A controller paired before this needs to re-pair (or re-read the endpoints) to see endpoint 7.

2026-11-12: Label agreed with the heating monitor (its TASK-40): the Boolean State sub-part's Descriptor TagList label must be exactly "Defrost" (the monitor matches it case-insensitively, as HEAT_PUMP_DEFROST_LABEL). The monitor subscribes to BooleanState StateValue on a wildcard endpoint of any Heat Pump node and reads the label at commissioning, so the adapter has to be re-paired to the monitor once this endpoint exists. That adds one attribute path to the monitor's subscription (13 paths in all for this node); check the adapter accepts it.
<!-- SECTION:NOTES:END -->
