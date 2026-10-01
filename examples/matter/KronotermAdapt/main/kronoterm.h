#pragma once

#include <stdint.h>

// Kronoterm "working function" (register 2001).
#define KRONOTERM_FUNCTION_HEATING 0
#define KRONOTERM_FUNCTION_DHW     1
#define KRONOTERM_FUNCTION_COOLING 2
#define KRONOTERM_FUNCTION_THERMAL_DISINFECTION 4

// Kronoterm "operation regime" (register 2007).
#define KRONOTERM_REGIME_COOLING 0
#define KRONOTERM_REGIME_HEATING 1
#define KRONOTERM_REGIME_OFF     2

// One snapshot of the heat pump, already scaled to SI units.
struct kronoterm_reading_t
{
    bool system_on;
    uint16_t working_function;
    uint16_t error_status; // 0 none, 1 warning, 2 error, 3 notification
    uint16_t regime;
    bool forced_dhw; // Fast DHW heating requested

    bool loop1_on;
    bool loop1_pump_on;
    float loop1_flow_temp_c;

    // False when no room thermostat is fitted to loop 1, in which case
    // loop1_room_temp_c holds the controller's -40 °C placeholder.
    bool loop1_room_temp_valid;
    float loop1_room_temp_c;
    float loop1_room_setpoint_c;

    float outdoor_temp_c;
    float flow_temp_c;   // Heat pump outlet
    float return_temp_c; // Heat pump inlet

    float dhw_temp_c;
    float dhw_setpoint_c;
    uint16_t dhw_operation; // 0 off, 1 on, 2 scheduled

    float power_w;
};

void kronoterm_read_task(void *arg);

// Implemented by main.cpp; publishes a reading to the Matter data model.
void matter_update_readings(const kronoterm_reading_t &reading);
