---
id: TASK-6
title: Switch the Matter examples from Wi-Fi to Thread
status: Done
assignee: []
created_date: '2026-10-01 12:53'
updated_date: '2026-10-01 13:12'
labels:
  - examples
dependencies: []
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The three Matter examples (examples/matter/SDM120M, SolaxX1G4, KronotermAdapt) currently build as Matter over Wi-Fi. The adapter's ESP32-C6 has an 802.15.4 radio, and the examples should join a Thread network instead: commission over BLE, then operate over Thread via a border router. Wi-Fi station support is no longer wanted in these builds.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Each of the three examples builds for esp32c6 with OpenThread enabled and Wi-Fi station disabled
- [x] #2 Each example's tracked sdkconfig and sdkconfig.defaults agree on the Thread configuration
- [x] #3 Each example configures the native 802.15.4 radio before starting Matter
- [x] #4 Each example README states that it uses Thread and needs a Thread border router
<!-- AC:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Switched examples/matter/SDM120M, SolaxX1G4 and KronotermAdapt from Matter over Wi-Fi to Matter over Thread.

- sdkconfig.defaults (all three): OpenThread enabled with SRP and DNS clients; Wi-Fi station and soft-AP disabled; the Wi-Fi-only lwIP route hooks and IPv6 autoconfig removed; IPv6 address count raised to 8; minimal mDNS replaced by extended discovery over SRP.
- main/main.cpp (all three): sets the OpenThread platform config (native 802.15.4 radio, no host connection, `nvs` storage) before esp_matter::start(), guarded by CHIP_DEVICE_CONFIG_ENABLE_THREAD.
- sdkconfig (tracked): SolaxX1G4 and KronotermAdapt matched their defaults exactly, so they were regenerated from the new defaults. SDM120M carries hand-tuned settings (trimmed cluster list, task watchdog off, one BLE connection), so only the Thread/Wi-Fi symbols were re-resolved and CONFIG_SUPPORT_THREAD_NETWORK_DIAGNOSTICS_CLUSTER was turned back on; the other customisations are unchanged.
- READMEs: state that the example runs over Thread, needs a border router, and joins as a router after BLE commissioning.

The resolved config is a full Thread device (CONFIG_OPENTHREAD_FTD=y), which esp-matter starts as a router; appropriate for mains-powered adapters.

Verification: all three build with ESP-IDF v5.5.4 for esp32c6 (app partition free space: SDM120M 22%, SolaxX1G4 16%, KronotermAdapt 15%). Builds were done in scratch build directories; the in-tree build/ directories are stale. Not flashed or commissioned on hardware.
<!-- SECTION:FINAL_SUMMARY:END -->
