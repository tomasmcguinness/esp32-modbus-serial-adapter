#pragma once

#include <stdio.h>
#include "driver/gpio.h"
#include "lvgl.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include <string.h>

class StatusDisplay
{
public:
    esp_err_t Init();

    void TurnOn();

    void UpdateDisplay(float voltage, float current, float power, float energy);
    void SetCommissioningCode(char *data, size_t size);
    void ClearCommissioningCode();

private:
    friend StatusDisplay &StatusDisplayMgr(void);
    static StatusDisplay sStatusDisplay;

    char *mCommissioningCode;

    lv_disp_t *mDisplayHandle;
    esp_lcd_panel_handle_t mPanelHandle;

    lv_obj_t *mQRCode;
    lv_obj_t *mVoltageLabel;
    lv_obj_t *mCurrentLabel;
    lv_obj_t *mPowerLabel;
    lv_obj_t *mEnergyLabel;
};

inline StatusDisplay &StatusDisplayMgr(void)
{
    return StatusDisplay::sStatusDisplay;
}
