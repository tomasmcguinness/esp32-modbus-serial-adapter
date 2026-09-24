---
id: TASK-2
title: Status LED reports commissioning state and Modbus activity
status: In Progress
assignee: []
created_date: '2026-08-26 09:27'
updated_date: '2026-08-26 09:35'
labels: []
dependencies: []
priority: medium
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The Revision A board carries a user LED (D2) on GPIO14 — driven high through R8 (5.1K) to GND, so active high — but the firmware never touches it. The only user-visible feedback today is the optional SSD1306 OLED behind CONFIG_ENABLE_STATUS_DISPLAY, which is off by default, so a board built without the display gives no indication of what it is doing.

Drive D2 so someone holding the device can tell, at a glance, whether it is waiting to be paired, mid-pairing, or commissioned and talking to the meter.

Agreed behaviour:
- Ready to be paired (commissioning window open, no fabric): slow blink, 1000 ms on / 1000 ms off.
- Pairing in progress (PASE session established): rapid blink, 100 ms on / 100 ms off.
- Commissioned/operational: dark, with a single 50 ms flash at the start of each SDM120 poll cycle.

Agreed scoping decisions: one flash per poll cycle rather than per register read; a failed Modbus read flashes identically to a successful one (failures stay in the logs only); the GPIO is hardcoded with no Kconfig option, matching how modbus.cpp hardcodes its Rev A pins.

Relevant local context for an implementer: commissioning state comes from app_event_cb in firmware/main/main.cpp, which already handles kCommissioningWindowOpened/Closed; esp-matter also posts the platform-specific kCommissioningSessionStarted/kCommissioningSessionStopped events (see managed_components/espressif__esp_matter/components/esp_matter/esp_matter.h) which fire on PASE establishment. Modbus polling lives in sdm120_read_task in firmware/main/sdm120.cpp. GPIO14 is free — Modbus uses 22/23/18 — and is not an ESP32-C6 strapping pin.

There is no test suite in this repo, so verification is a build plus on-device observation.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 An uncommissioned board with its commissioning window open blinks D2 slowly at 1000 ms on / 1000 ms off
- [ ] #2 D2 switches to a rapid 100 ms on / 100 ms off blink as soon as a controller establishes a pairing (PASE) session
- [ ] #3 Once commissioning completes, D2 goes dark and emits a single 50 ms flash at the start of every SDM120 poll cycle
- [ ] #4 A pairing attempt that is aborted or fails returns D2 to the slow blink rather than leaving it blinking rapidly
- [ ] #5 Removing the fabric from the controller reopens the commissioning window and returns D2 to the slow blink
- [ ] #6 A poll cycle in which Modbus reads fail flashes D2 identically to a successful cycle, so the blip continues with no meter attached
- [ ] #7 The LED state is driven without blocking the Matter event thread or the Modbus polling task
- [x] #8 The firmware builds clean for esp32c6 with idf.py build, both with CONFIG_ENABLE_STATUS_DISPLAY on and off
- [x] #9 README documents the three LED patterns and the GPIO the LED is on
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. Add firmware/main/status_led.h — status_led_state_t enum (OFF / READY_TO_PAIR / PAIRING / OPERATIONAL) plus status_led_init(), status_led_set_state(), status_led_flash().
2. Add firmware/main/status_led.cpp — GPIO14 hardcoded active high; one small FreeRTOS task driving the pattern from a volatile state variable, using ulTaskNotifyTake as both the blink delay and the flash trigger so state changes and flashes take effect immediately and neither caller blocks.
3. main.cpp — call status_led_init() in app_main; map kCommissioningWindowOpened to READY_TO_PAIR, kCommissioningSessionStarted to PAIRING, kCommissioningSessionStopped to READY_TO_PAIR or OPERATIONAL by fabric count, kCommissioningComplete to OPERATIONAL, kCommissioningWindowClosed to OPERATIONAL or OFF by fabric count.
4. sdm120.cpp — status_led_flash() at the top of each poll cycle in sdm120_read_task.
5. main/CMakeLists.txt — add status_led.cpp to SRCS unconditionally.
6. README — document the three LED patterns and GPIO14.
7. Verify: idf.py build for esp32c6 with the display option off and on; then on-device observation of the pairing, commissioned and fabric-removal transitions.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Implemented across firmware/main/status_led.{h,cpp} (new), main.cpp, sdm120.cpp, CMakeLists.txt and README.md.

status_led.cpp drives GPIO14 from a single 2048-byte FreeRTOS task at priority 5. The task reads a volatile status_led_state_t and uses ulTaskNotifyTake as both the blink half-period delay and the flash trigger, so a state change applies immediately rather than after the current half-period, and both status_led_set_state() (Matter event thread) and status_led_flash() (Modbus poll task) return without blocking. In OPERATIONAL the task blocks on portMAX_DELAY and re-reads the state on wake to tell a flash request from a state change.

The LED starts in OPERATIONAL (dark). An uncommissioned board receives kCommissioningWindowOpened moments after esp_matter::start() and drops into the slow blink on its own, so no boot-time fabric lookup is needed. A helper is_commissioned() wraps the FabricCount() > 0 check used by the kCommissioningSessionStopped and kCommissioningWindowClosed cases.

Build verification (AC #8): both configurations build clean for esp32c6 on ESP-IDF v5.5.4 — the default config in firmware/build, and CONFIG_ENABLE_STATUS_DISPLAY=y in a separate build directory.

Note for whoever picks this up: CONFIG_LV_USE_QRCODE=y is present in the checked-in sdkconfig but missing from sdkconfig.defaults, so a display-enabled build configured from defaults alone fails in status_display.cpp on lv_qrcode_create. Pre-existing, unrelated to this task, but it will bite anyone who regenerates the config.

AC #1-#7 describe on-device behaviour and remain unverified — they need a board on the bench and a Matter controller.
<!-- SECTION:NOTES:END -->
