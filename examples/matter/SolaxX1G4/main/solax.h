#pragma once

#include <stdint.h>

// One snapshot of the inverter, already scaled to SI units. Signs follow the
// Solax convention: AC and PV power are positive when generating, battery
// power is positive when charging.
struct solax_reading_t
{
    float grid_voltage_v;
    float grid_current_a;
    float grid_power_w;

    float pv1_voltage_v;
    float pv1_current_a;
    float pv1_power_w;

    float pv2_voltage_v;
    float pv2_current_a;
    float pv2_power_w;

    float battery_voltage_v;
    float battery_current_a;
    float battery_power_w;
    uint8_t battery_soc_percent;
};

void solax_read_task(void *arg);

// Implemented by main.cpp; publishes a reading to the Matter data model.
void matter_update_readings(const solax_reading_t &reading);
