#pragma once
#include "esp_err.h"
#include "esp_lcd_types.h"
#include "esp_lcd_touch.h"
#include "lvgl.h"
#include "zenith_registry.h"


typedef struct zenith_ui_config {
    // Shared SPI 
    spi_host_device_t spi_host;
    int mosi_pin;
    int miso_pin;
    int sclk_pin;
    
    // LCD-specific
    int lcd_cs_pin;
    int lcd_dc_pin;
    int lcd_reset_pin;
    int lcd_backlight_pin;
    
    // Touch-specific
    int touch_cs_pin;
    int touch_irq_pin;
    
    // Handle for data
    zenith_registry_handle_t node_registry;
} zenith_ui_config_t;

typedef struct zenith_ui_s zenith_ui_t;
typedef zenith_ui_t *zenith_ui_handle_t;

esp_err_t zenith_ui_new_core(const zenith_ui_config_t* config, zenith_ui_handle_t *ui);

struct zenith_ui_s {
    esp_err_t (*del)(zenith_ui_handle_t tp);
    esp_lcd_panel_io_handle_t lcd_io_handle;
    esp_lcd_panel_handle_t lcd_panel_handle;
    esp_lcd_panel_io_handle_t touch_io_handle;
    esp_lcd_touch_handle_t touch_panel_handle;
    lv_display_t *display;
    zenith_ui_config_t config;
};

esp_err_t zenith_ui_del(zenith_ui_handle_t ui);
esp_err_t zenith_ui_core_fade_lcd_brightness(const int brightness_percentage, const uint32_t max_fade_time);