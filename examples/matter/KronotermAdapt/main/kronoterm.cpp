#include "kronoterm.h"
#include "modbus.h"
#include "status_led.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "Kronoterm";

// Holding registers (function code 0x03) from the Kronoterm heat pump manager
// Modbus map. Kronoterm numbers registers from 1, so the address sent on the
// wire is one less than the numbers below. All values are S16.
#define REG_SYSTEM_OPERATION   2000 // 0 off, 1 on
#define REG_WORKING_FUNCTION   2001 // KRONOTERM_FUNCTION_*
#define REG_ERROR_STATUS       2006 // 0 none, 1 warning, 2 error, 3 notification
#define REG_OPERATION_REGIME   2007 // KRONOTERM_REGIME_*
#define REG_FORCED_DHW         2010 // 0 off, 1 fast DHW heating active
#define REG_DHW_SETPOINT       2023 // 0.1 °C, desired DHW temperature
#define REG_DHW_CURRENT_SETPOINT 2024 // 0.1 °C, including ECO/comfort offsets
#define REG_DHW_OPERATION      2026 // 0 off, 1 on, 2 scheduled
#define REG_LOOP1_OPERATION    2042 // 0 off, 1 on, 2 scheduled
#define REG_LOOP1_PUMP         2045 // 0 off, 1 on
#define REG_RETURN_TEMP        2101 // 0.1 °C, HP inlet
#define REG_DHW_TEMP           2102 // 0.1 °C, DHW tank
#define REG_OUTDOOR_TEMP       2103 // 0.1 °C
#define REG_FLOW_TEMP          2104 // 0.1 °C, HP outlet
#define REG_POWER              2129 // 1 W, current power consumption
#define REG_LOOP1_FLOW_TEMP    2130 // 0.1 °C
#define REG_LOOP1_ROOM_TEMP    2160 // 0.1 °C, -40 °C when no thermostat is fitted
#define REG_LOOP1_ROOM_SETPOINT 2191 // 0.1 °C, current desired room temperature

#define TEMP_SCALE             0.1f
#define ROOM_TEMP_MISSING_C    -40.0f

#define POLL_INTERVAL_MS       10000

// The registers are spread out, and some controllers reject reads that span
// undefined registers, so each poll reads a few small contiguous blocks.
struct register_block_t
{
    uint16_t first;
    uint16_t count;
    uint16_t regs[12];
};

static register_block_t blocks[] = {
    { REG_SYSTEM_OPERATION, REG_FORCED_DHW - REG_SYSTEM_OPERATION + 1, {} },
    { REG_DHW_SETPOINT, REG_DHW_CURRENT_SETPOINT - REG_DHW_SETPOINT + 1, {} },
    { REG_DHW_OPERATION, 1, {} },
    { REG_LOOP1_OPERATION, REG_LOOP1_PUMP - REG_LOOP1_OPERATION + 1, {} },
    { REG_RETURN_TEMP, REG_FLOW_TEMP - REG_RETURN_TEMP + 1, {} },
    { REG_POWER, REG_LOOP1_FLOW_TEMP - REG_POWER + 1, {} },
    { REG_LOOP1_ROOM_TEMP, 1, {} },
    { REG_LOOP1_ROOM_SETPOINT, 1, {} },
};

static int16_t reg(uint16_t number)
{
    for (const register_block_t &b : blocks)
    {
        if (number >= b.first && number < b.first + b.count)
        {
            return (int16_t)b.regs[number - b.first];
        }
    }
    ESP_LOGE(TAG, "Register %u is not in any polled block", number);
    return 0;
}

static float temp(uint16_t number)
{
    return reg(number) * TEMP_SCALE;
}

static esp_err_t read_blocks()
{
    for (register_block_t &b : blocks)
    {
        esp_err_t err = modbus_read_holding_registers(b.first - 1, b.count, b.regs);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to read registers %u-%u", b.first, b.first + b.count - 1);
            return err;
        }
    }
    return ESP_OK;
}

static void decode(kronoterm_reading_t *r)
{
    r->system_on = reg(REG_SYSTEM_OPERATION) != 0;
    r->working_function = reg(REG_WORKING_FUNCTION);
    r->error_status = reg(REG_ERROR_STATUS);
    r->regime = reg(REG_OPERATION_REGIME);
    r->forced_dhw = reg(REG_FORCED_DHW) != 0;

    r->loop1_on = reg(REG_LOOP1_OPERATION) != 0;
    r->loop1_pump_on = reg(REG_LOOP1_PUMP) != 0;
    r->loop1_flow_temp_c = temp(REG_LOOP1_FLOW_TEMP);

    r->loop1_room_temp_c = temp(REG_LOOP1_ROOM_TEMP);
    r->loop1_room_temp_valid = r->loop1_room_temp_c > ROOM_TEMP_MISSING_C;
    r->loop1_room_setpoint_c = temp(REG_LOOP1_ROOM_SETPOINT);

    r->outdoor_temp_c = temp(REG_OUTDOOR_TEMP);
    r->flow_temp_c = temp(REG_FLOW_TEMP);
    r->return_temp_c = temp(REG_RETURN_TEMP);

    r->dhw_temp_c = temp(REG_DHW_TEMP);
    r->dhw_setpoint_c = temp(REG_DHW_CURRENT_SETPOINT);
    r->dhw_operation = reg(REG_DHW_OPERATION);

    r->power_w = reg(REG_POWER);
}

void kronoterm_read_task(void *arg)
{
    for (const register_block_t &b : blocks)
    {
        configASSERT(b.count <= sizeof(b.regs) / sizeof(b.regs[0]));
    }

    vTaskDelay(pdMS_TO_TICKS(2000)); // Wait for serial to settle

    while (1)
    {
        ESP_LOGI(TAG, "Reading Kronoterm heat pump...");

        status_led_flash();

        if (read_blocks() == ESP_OK)
        {
            kronoterm_reading_t reading;
            decode(&reading);

            ESP_LOGI(TAG, "System:  %s  function %u  regime %u  error status %u", reading.system_on ? "on" : "off",
                     reading.working_function, reading.regime, reading.error_status);
            ESP_LOGI(TAG, "Power:   %.0f W", reading.power_w);
            ESP_LOGI(TAG, "Temps:   outdoor %.1f C  flow %.1f C  return %.1f C", reading.outdoor_temp_c,
                     reading.flow_temp_c, reading.return_temp_c);
            ESP_LOGI(TAG, "Loop 1:  %s  pump %s  flow %.1f C  room %.1f C%s  setpoint %.1f C",
                     reading.loop1_on ? "on" : "off", reading.loop1_pump_on ? "on" : "off", reading.loop1_flow_temp_c,
                     reading.loop1_room_temp_c, reading.loop1_room_temp_valid ? "" : " (no sensor)",
                     reading.loop1_room_setpoint_c);
            ESP_LOGI(TAG, "DHW:     operation %u  tank %.1f C  setpoint %.1f C%s", reading.dhw_operation,
                     reading.dhw_temp_c, reading.dhw_setpoint_c, reading.forced_dhw ? "  (fast heating)" : "");

            matter_update_readings(reading);
        }
        else
        {
            ESP_LOGE(TAG, "Failed to read heat pump registers; keeping last values");
        }

        ESP_LOGI(TAG, "---");
        vTaskDelay(pdMS_TO_TICKS(POLL_INTERVAL_MS));
    }
}
