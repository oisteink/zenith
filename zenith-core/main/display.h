#pragma once

#include "esp_err.h"
#include "lvgl.h"

// Using SPI2 in the example
#define LCD_HOST  SPI2_HOST

// Display configuration
#define LCD_PIXEL_CLOCK_HZ     (20 * 1000 * 1000)
#define PIN_NUM_SCL            4
#define PIN_NUM_SDA            5
#define PIN_NUM_LCD_DC         6
#define PIN_NUM_LCD_CS         7
#define PIN_NUM_LCD_RST        0

// The pixel number in horizontal and vertical
#define LCD_H_RES              240
#define LCD_V_RES              240

// Bit number used to represent command and parameter
#define LCD_CMD_BITS           8
#define LCD_PARAM_BITS         8

#define LVGL_DRAW_BUF_LINES    20 // number of display lines in each draw buffer
#define LVGL_TICK_PERIOD_MS    2
#define LVGL_TASK_MAX_DELAY_MS 500
#define LVGL_TASK_MIN_DELAY_MS 1
#define LVGL_TASK_STACK_SIZE   (4 * 1024)
#define LVGL_TASK_PRIORITY     2

esp_err_t display_init(void);
void display_update_values(float temperature, float humidity);