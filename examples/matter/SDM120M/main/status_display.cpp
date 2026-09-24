#include <stdio.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "status_display.h"

#include "driver/i2c_master.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lvgl_port.h"

#include "lvgl.h"

static const char *TAG = "status_display";

#define I2C_BUS_PORT 0

#define EXAMPLE_LCD_PIXEL_CLOCK_HZ (400 * 1000)
#define EXAMPLE_PIN_NUM_SDA        22
#define EXAMPLE_PIN_NUM_SCL        23
#define EXAMPLE_PIN_NUM_RST        2
#define EXAMPLE_I2C_HW_ADDR        0x3C

#define EXAMPLE_LCD_CMD_BITS   8
#define EXAMPLE_LCD_PARAM_BITS 8

#define EXAMPLE_LCD_H_RES 128
#define EXAMPLE_LCD_V_RES 64

StatusDisplay StatusDisplay::sStatusDisplay;

esp_err_t StatusDisplay::Init()
{
    ESP_LOGI(TAG, "StatusDisplay::Init()");

    ESP_LOGI(TAG, "Initialize I2C bus");
    i2c_master_bus_handle_t i2c_bus = NULL;
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_BUS_PORT,
        .sda_io_num = (gpio_num_t)EXAMPLE_PIN_NUM_SDA,
        .scl_io_num = (gpio_num_t)EXAMPLE_PIN_NUM_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags = {
            .enable_internal_pullup = true,
        }};
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &i2c_bus));

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t io_config = {
        .dev_addr = EXAMPLE_I2C_HW_ADDR,
        .control_phase_bytes = 1,
        .dc_bit_offset = 6,
        .lcd_cmd_bits = EXAMPLE_LCD_CMD_BITS,
        .lcd_param_bits = EXAMPLE_LCD_CMD_BITS,
        .scl_speed_hz = EXAMPLE_LCD_PIXEL_CLOCK_HZ,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus, &io_config, &io_handle));

    ESP_LOGI(TAG, "Install SSD1306 panel driver");
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = EXAMPLE_PIN_NUM_RST,
        .bits_per_pixel = 1,
    };

    esp_lcd_panel_ssd1306_config_t ssd1306_config = {
        .height = EXAMPLE_LCD_V_RES,
    };
    panel_config.vendor_config = &ssd1306_config;
    ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(io_handle, &panel_config, &mPanelHandle));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(mPanelHandle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(mPanelHandle));

    ESP_LOGI(TAG, "Initialize LVGL");
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    lvgl_port_init(&lvgl_cfg);

    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI(TAG, "Create LVGL Display");

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = mPanelHandle,
        .buffer_size = EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES,
        .double_buffer = false,
        .hres = EXAMPLE_LCD_H_RES,
        .vres = EXAMPLE_LCD_V_RES,
        .monochrome = true,
        .rotation = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
        }};

    mDisplayHandle = lvgl_port_add_disp(&disp_cfg);

    lv_disp_set_rotation(mDisplayHandle, LV_DISP_ROT_180);

    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI(TAG, "Create LVGL Screen");

    lv_obj_t *scr = lv_scr_act();

    ESP_LOGI(TAG, "LVGL Screen activated");

    vTaskDelay(pdMS_TO_TICKS(100));

    mQRCode = lv_qrcode_create(scr, 60,  lv_color_hex3(0xFF), lv_color_hex3(0x00));
    lv_obj_align(mQRCode, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(mQRCode, LV_OBJ_FLAG_HIDDEN);

    mVoltageLabel = lv_label_create(scr);
    lv_label_set_text(mVoltageLabel, "V: --");
    lv_obj_set_width(mVoltageLabel, 128);
    lv_obj_align(mVoltageLabel, LV_ALIGN_TOP_LEFT, 0, 0);
    
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI(TAG, "Voltage label finished");

    mCurrentLabel = lv_label_create(scr);
    lv_label_set_text(mCurrentLabel, "A: --");
    lv_obj_set_width(mCurrentLabel, 128);
    lv_obj_align(mCurrentLabel, LV_ALIGN_TOP_LEFT, 0, 16);

    ESP_LOGI(TAG, "Current label finished");

    vTaskDelay(10 / portTICK_PERIOD_MS);

    mPowerLabel = lv_label_create(scr);
    lv_label_set_text(mPowerLabel, "W: --");
    lv_obj_set_width(mPowerLabel, 128);
    lv_obj_align(mPowerLabel, LV_ALIGN_TOP_LEFT, 0, 32);

    ESP_LOGI(TAG, "Power label finished");

    vTaskDelay(10 / portTICK_PERIOD_MS);

    mEnergyLabel = lv_label_create(scr);
    lv_label_set_text(mEnergyLabel, "kWh: --");
    lv_obj_set_width(mEnergyLabel, 128);
    lv_obj_align(mEnergyLabel, LV_ALIGN_TOP_LEFT, 0, 48);

    vTaskDelay(10 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "StatusDisplay::Init() finished");

    return ESP_OK;
}

void StatusDisplay::TurnOn()
{
    ESP_LOGI(TAG, "Turning display on");
    esp_lcd_panel_disp_on_off(mPanelHandle, true);
}

void StatusDisplay::ClearCommissioningCode()
{
    ESP_LOGI(TAG, "Clearing commissioning code");

    lvgl_port_lock(0);
    lv_obj_add_flag(mQRCode, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(mVoltageLabel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(mCurrentLabel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(mPowerLabel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(mEnergyLabel, LV_OBJ_FLAG_HIDDEN);
    lvgl_port_unlock();
}

void StatusDisplay::SetCommissioningCode(char *qrCode, size_t size)
{
    ESP_LOGI(TAG, "Set QR CODE [%d] %s", size, qrCode);
    mCommissioningCode = (char *)calloc(1, size + 1);
    memcpy(mCommissioningCode, qrCode, size);

    lvgl_port_lock(0);
    lv_qrcode_update(mQRCode, mCommissioningCode, strlen(mCommissioningCode));
    lv_obj_clear_flag(mQRCode, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(mVoltageLabel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(mCurrentLabel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(mPowerLabel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(mEnergyLabel, LV_OBJ_FLAG_HIDDEN);
    lvgl_port_unlock();

    free(mCommissioningCode);
}

extern "C" void status_display_update(float voltage, float current, float power, float energy)
{
    StatusDisplayMgr().UpdateDisplay(voltage, current, power, energy);
}

void StatusDisplay::UpdateDisplay(float voltage, float current, float power, float energy)
{
    ESP_LOGI(TAG, "Updating the display");

    char buf[32];

    snprintf(buf, sizeof(buf), "V: %.1f", voltage);
    lv_label_set_text(mVoltageLabel, buf);

    snprintf(buf, sizeof(buf), "A: %.2f", current);
    lv_label_set_text(mCurrentLabel, buf);

    snprintf(buf, sizeof(buf), "W: %.1f", power);
    lv_label_set_text(mPowerLabel, buf);

    snprintf(buf, sizeof(buf), "kWh: %.2f", energy);
    lv_label_set_text(mEnergyLabel, buf);
}
