#include "modbus.h"
#include "sdm120.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "Main";

void app_main(void)
{
    modbus_uart_init();
    ESP_LOGI(TAG, "Modbus UART initialized");

    xTaskCreate(sdm120_read_task, "sdm120m_read", 2048, NULL, 5, NULL);
}