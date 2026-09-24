#include "solax.h"
#include "modbus.h"
#include "status_led.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "Solax";

// Input registers (function code 0x04) from the Solax Hybrid X1/X3-G4 Modbus
// RTU protocol. Everything needed lives in one contiguous block starting at
// 0x0000, so a single request per poll covers it.
#define REG_GRID_VOLTAGE    0x0000 // U16, 0.1 V
#define REG_GRID_CURRENT    0x0001 // S16, 0.1 A
#define REG_GRID_POWER      0x0002 // S16, 1 W (inverter AC output)
#define REG_PV1_VOLTAGE     0x0003 // U16, 0.1 V
#define REG_PV2_VOLTAGE     0x0004 // U16, 0.1 V
#define REG_PV1_CURRENT     0x0005 // U16, 0.1 A
#define REG_PV2_CURRENT     0x0006 // U16, 0.1 A
#define REG_PV1_POWER       0x000A // U16, 1 W
#define REG_PV2_POWER       0x000B // U16, 1 W
#define REG_BATTERY_VOLTAGE 0x0014 // S16, 0.1 V
#define REG_BATTERY_CURRENT 0x0015 // S16, 0.1 A, positive when charging
#define REG_BATTERY_POWER   0x0016 // S16, 1 W, positive when charging
#define REG_BATTERY_SOC     0x001C // U16, 1 %

#define REG_BLOCK_START     REG_GRID_VOLTAGE
#define REG_BLOCK_COUNT     (REG_BATTERY_SOC - REG_BLOCK_START + 1)

#define POLL_INTERVAL_MS    5000

static float u16(const uint16_t *regs, uint16_t reg, float scale)
{
    return regs[reg - REG_BLOCK_START] * scale;
}

static float s16(const uint16_t *regs, uint16_t reg, float scale)
{
    return (int16_t)regs[reg - REG_BLOCK_START] * scale;
}

static void decode(const uint16_t *regs, solax_reading_t *r)
{
    r->grid_voltage_v = u16(regs, REG_GRID_VOLTAGE, 0.1f);
    r->grid_current_a = s16(regs, REG_GRID_CURRENT, 0.1f);
    r->grid_power_w = s16(regs, REG_GRID_POWER, 1.0f);

    r->pv1_voltage_v = u16(regs, REG_PV1_VOLTAGE, 0.1f);
    r->pv1_current_a = u16(regs, REG_PV1_CURRENT, 0.1f);
    r->pv1_power_w = u16(regs, REG_PV1_POWER, 1.0f);

    r->pv2_voltage_v = u16(regs, REG_PV2_VOLTAGE, 0.1f);
    r->pv2_current_a = u16(regs, REG_PV2_CURRENT, 0.1f);
    r->pv2_power_w = u16(regs, REG_PV2_POWER, 1.0f);

    r->battery_voltage_v = s16(regs, REG_BATTERY_VOLTAGE, 0.1f);
    r->battery_current_a = s16(regs, REG_BATTERY_CURRENT, 0.1f);
    r->battery_power_w = s16(regs, REG_BATTERY_POWER, 1.0f);

    uint16_t soc = regs[REG_BATTERY_SOC - REG_BLOCK_START];
    r->battery_soc_percent = soc > 100 ? 100 : (uint8_t)soc;
}

void solax_read_task(void *arg)
{
    vTaskDelay(pdMS_TO_TICKS(2000)); // Wait for serial to settle

    uint16_t regs[REG_BLOCK_COUNT];

    while (1)
    {
        ESP_LOGI(TAG, "Reading Solax X1-G4...");

        status_led_flash();

        if (modbus_read_input_registers(REG_BLOCK_START, REG_BLOCK_COUNT, regs) == ESP_OK)
        {
            solax_reading_t reading;
            decode(regs, &reading);

            ESP_LOGI(TAG, "AC:      %.1f V  %.1f A  %.0f W", reading.grid_voltage_v, reading.grid_current_a, reading.grid_power_w);
            ESP_LOGI(TAG, "PV1:     %.1f V  %.1f A  %.0f W", reading.pv1_voltage_v, reading.pv1_current_a, reading.pv1_power_w);
            ESP_LOGI(TAG, "PV2:     %.1f V  %.1f A  %.0f W", reading.pv2_voltage_v, reading.pv2_current_a, reading.pv2_power_w);
            ESP_LOGI(TAG, "Battery: %.1f V  %.1f A  %.0f W  %u%%", reading.battery_voltage_v, reading.battery_current_a,
                     reading.battery_power_w, reading.battery_soc_percent);

            matter_update_readings(reading);
        }
        else
        {
            ESP_LOGE(TAG, "Failed to read inverter registers; keeping last values");
        }

        ESP_LOGI(TAG, "---");
        vTaskDelay(pdMS_TO_TICKS(POLL_INTERVAL_MS));
    }
}
