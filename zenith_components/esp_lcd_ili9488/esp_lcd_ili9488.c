/*
 * SPDX-FileCopyrightText: 2022 atanisoft (github.com/atanisoft)
 *
 * SPDX-License-Identifier: MIT
 */

#include <driver/gpio.h>
#include <esp_lcd_panel_interface.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_commands.h>
#include <esp_log.h>
#include <esp_rom_gpio.h>
#include <esp_check.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <memory.h>
#include <stdlib.h>
#include <sys/cdefs.h>

static const char *TAG = "ili9488";

typedef struct
{
    uint8_t cmd;
    uint8_t data[16];
    uint8_t data_bytes;
} lcd_init_cmd_t;

typedef struct
{
    esp_lcd_panel_t base;
    esp_lcd_panel_io_handle_t io;
    int reset_gpio_num;
    bool reset_level;
    int x_gap;
    int y_gap;
    uint8_t memory_access_control;
    uint8_t color_mode;
    uint8_t bits_per_pixel;
} ili9488_panel_t;

enum ili9488_constants {
    ILI9488_INTRFC_MODE_CTL = 0xB0,
    ILI9488_FRAME_RATE_NORMAL_CTL = 0xB1,
    ILI9488_INVERSION_CTL = 0xB4,
    ILI9488_FUNCTION_CTL = 0xB6,
    ILI9488_ENTRY_MODE_CTL = 0xB7,
    ILI9488_POWER_CTL_ONE = 0xC0,
    ILI9488_POWER_CTL_TWO = 0xC1,
    ILI9488_VCOM_CTL = 0xC5,
    ILI9488_POSITIVE_GAMMA_CTL = 0xE0,
    ILI9488_NEGATIVE_GAMMA_CTL = 0xE1,
    ILI9488_ADJUST_CTL_THREE = 0xF7,

    ILI9488_COLOR_MODE_16BIT = 0x55,
    ILI9488_COLOR_MODE_18BIT = 0x66,

    ILI9488_INTERFACE_MODE_USE_SDO = 0x00,
    ILI9488_INTERFACE_MODE_IGNORE_SDO = 0x80,

    ILI9488_IMAGE_FUNCTION_DISABLE_24BIT_DATA = 0x00,

    ILI9488_WRITE_MODE_BCTRL_DD_ON = 0x28,
    ILI9488_FRAME_RATE_60HZ = 0xA0,

    ILI9488_INVERSION_1_DOT = 0x01,
    ILI9488_INVERSION_2_DOT = 0x02,

    ILI9488_INIT_LENGTH_MASK = 0x1F,
    ILI9488_INIT_DONE_FLAG = 0xFF
};

static esp_err_t panel_ili9488_del(esp_lcd_panel_t *panel)
{
    ili9488_panel_t *ili9488 = __containerof(panel, ili9488_panel_t, base);

    if (ili9488->reset_gpio_num >= 0)
    {
        gpio_reset_pin(ili9488->reset_gpio_num);
    }

    ESP_LOGI(TAG, "del ili9488 panel @%p", ili9488);
    free(ili9488);
    return ESP_OK;
}

static esp_err_t panel_ili9488_reset(esp_lcd_panel_t *panel)
{
    ili9488_panel_t *ili9488 = __containerof(panel, ili9488_panel_t, base);
    esp_lcd_panel_io_handle_t io = ili9488->io;

    if (ili9488->reset_gpio_num >= 0)
    {
        ESP_LOGI(TAG, "Setting GPIO:%d to %d", ili9488->reset_gpio_num, ili9488->reset_level);
        // perform hardware reset
        gpio_set_level(ili9488->reset_gpio_num, ili9488->reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
        ESP_LOGI(TAG, "Setting GPIO:%d to %d", ili9488->reset_gpio_num, !ili9488->reset_level);
        gpio_set_level(ili9488->reset_gpio_num, !ili9488->reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    else
    {
        ESP_LOGI(TAG, "Sending SW_RESET to display");
        esp_lcd_panel_io_tx_param(io, LCD_CMD_SWRESET, NULL, 0);
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    return ESP_OK;
}

static esp_err_t panel_ili9488_init(esp_lcd_panel_t *panel)
{
    ili9488_panel_t *ili9488 = __containerof(panel, ili9488_panel_t, base);
    esp_lcd_panel_io_handle_t io = ili9488->io;

    lcd_init_cmd_t ili9488_init[] = {
        // Initial gamma is OK { ILI9488_POSITIVE_GAMMA_CTL, { 0x00, 0x03, 0x09, 0x08, 0x16, 0x0A, 0x3F, 0x78, 0x4C, 0x09, 0x0A, 0x08, 0x16, 0x1A, 0x0F }, 15 },
        // Initial gamma is OK { ILI9488_NEGATIVE_GAMMA_CTL, { 0x00, 0x16, 0x19, 0x03, 0x0F, 0x05, 0x32, 0x45, 0x46, 0x04, 0x0E, 0x0D, 0x35, 0x37, 0x0F}, 15 },
        { ILI9488_POWER_CTL_ONE, { 0x17, 0x15 }, 2 },  // Positive gamma = 5v, Negative gamma = -4.8750v
        { ILI9488_POWER_CTL_TWO, { 0x41 }, 1 },        // VGH = VCI x 6, VGL = -(VCI x 4)
        { ILI9488_VCOM_CTL, { 0x00, 0x12, 0x80 }, 3 }, // -1.71875
        { LCD_CMD_MADCTL, { ili9488->memory_access_control }, 1 },
        { LCD_CMD_COLMOD, { ili9488->color_mode }, 1 },
        { ILI9488_INTRFC_MODE_CTL, { ILI9488_INTERFACE_MODE_USE_SDO }, 1 }, // MISO is not proper tri-state on my screen: do not wire
        { ILI9488_FRAME_RATE_NORMAL_CTL, { ILI9488_FRAME_RATE_60HZ }, 1 },
        { ILI9488_INVERSION_CTL, { ILI9488_INVERSION_2_DOT }, 1 },
        { ILI9488_FUNCTION_CTL, { 0x02, 0x02, 0x3B }, 3 }, //VCOM in non-display area is set to AGND, Interval Scan Cycle 5 frames / 84ms, Number of Lines (8 x 0x3B + 1) 480
        { ILI9488_ENTRY_MODE_CTL, { 0xC6 }, 1 }, //Conditional copy 16 to 18bit, GON/DTE is Normal display, Enable Low voltage detection control
        { ILI9488_ADJUST_CTL_THREE, { 0xA9, 0x51, 0x2C, 0x02 }, 4 }, //DSI write DCS command, use stream packet RGB 666
        { LCD_CMD_NOP, { 0 }, ILI9488_INIT_DONE_FLAG },
    };

    ESP_LOGI(TAG, "Initializing ILI9488");
    int cmd = 0;
    while (ili9488_init[cmd].data_bytes != ILI9488_INIT_DONE_FLAG)
    {
        ESP_LOGD(TAG, "Sending CMD: %02x, len: %d", ili9488_init[cmd].cmd, ili9488_init[cmd].data_bytes & ILI9488_INIT_LENGTH_MASK);
        esp_lcd_panel_io_tx_param(io, ili9488_init[cmd].cmd, ili9488_init[cmd].data, ili9488_init[cmd].data_bytes & ILI9488_INIT_LENGTH_MASK);
        cmd++;
    }

    // Take the display out of sleep mode.
    esp_lcd_panel_io_tx_param(io, LCD_CMD_SLPOUT, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(100));

    // Turn on the display.
    esp_lcd_panel_io_tx_param(io, LCD_CMD_DISPON, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI(TAG, "Initialization complete");

    return ESP_OK;
}

#define SEND_COORDS(start, end, io, cmd)                                                                                                                                                               \
    esp_lcd_panel_io_tx_param(io, cmd,                                                                                                                                                                 \
        (uint8_t[]) {                                                                                                                                                                                  \
            (start >> 8) & 0xFF,                                                                                                                                                                       \
            start & 0xFF,                                                                                                                                                                              \
            ((end - 1) >> 8) & 0xFF,                                                                                                                                                                   \
            (end - 1) & 0xFF,                                                                                                                                                                          \
        },                                                                                                                                                                                             \
        4)

static esp_err_t panel_ili9488_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end, const void *color_data)
{
    ili9488_panel_t *ili9488 = __containerof(panel, ili9488_panel_t, base);
    assert((x_start < x_end) && (y_start < y_end) && "starting position must be smaller than end position");
    esp_lcd_panel_io_handle_t io = ili9488->io;

    x_start += ili9488->x_gap;
    x_end += ili9488->x_gap;
    y_start += ili9488->y_gap;
    y_end += ili9488->y_gap;

    size_t color_data_len = (x_end - x_start) * (y_end - y_start) * (ili9488->bits_per_pixel / 8);
    ESP_RETURN_ON_ERROR(SEND_COORDS(x_start, x_end, io, LCD_CMD_CASET), TAG, "error setting column address");
    ESP_RETURN_ON_ERROR(SEND_COORDS(y_start, y_end, io, LCD_CMD_RASET), TAG, "error setting page address");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_color(io, LCD_CMD_RAMWR, color_data, color_data_len), TAG, "error during memory write");

    return ESP_OK;
}

#undef SEND_COORDS

static esp_err_t panel_ili9488_invert_color(esp_lcd_panel_t *panel, bool invert_color_data)
{
    ili9488_panel_t *ili9488 = __containerof(panel, ili9488_panel_t, base);
    esp_lcd_panel_io_handle_t io = ili9488->io;

    if (invert_color_data)
    {
        esp_lcd_panel_io_tx_param(io, LCD_CMD_INVON, NULL, 0);
    }
    else
    {
        esp_lcd_panel_io_tx_param(io, LCD_CMD_INVOFF, NULL, 0);
    }

    return ESP_OK;
}

static esp_err_t panel_ili9488_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y)
{
    ili9488_panel_t *ili9488 = __containerof(panel, ili9488_panel_t, base);
    esp_lcd_panel_io_handle_t io = ili9488->io;
    if (mirror_x)
    {
        ili9488->memory_access_control &= ~LCD_CMD_MX_BIT;
    }
    else
    {
        ili9488->memory_access_control |= LCD_CMD_MX_BIT;
    }
    if (mirror_y)
    {
        ili9488->memory_access_control |= LCD_CMD_MY_BIT;
    }
    else
    {
        ili9488->memory_access_control &= ~LCD_CMD_MY_BIT;
    }
    esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, &ili9488->memory_access_control, 1);
    return ESP_OK;
}

static esp_err_t panel_ili9488_swap_xy(esp_lcd_panel_t *panel, bool swap_axes)
{
    ili9488_panel_t *ili9488 = __containerof(panel, ili9488_panel_t, base);
    esp_lcd_panel_io_handle_t io = ili9488->io;
    if (swap_axes)
    {
        ili9488->memory_access_control |= LCD_CMD_MV_BIT;
    }
    else
    {
        ili9488->memory_access_control &= ~LCD_CMD_MV_BIT;
    }
    esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, &ili9488->memory_access_control, 1);
    return ESP_OK;
}

static esp_err_t panel_ili9488_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap)
{
    ili9488_panel_t *ili9488 = __containerof(panel, ili9488_panel_t, base);
    ili9488->x_gap = x_gap;
    ili9488->y_gap = y_gap;
    return ESP_OK;
}

static esp_err_t panel_ili9488_disp_on_off(esp_lcd_panel_t *panel, bool on_off)
{
    ili9488_panel_t *ili9488 = __containerof(panel, ili9488_panel_t, base);
    esp_lcd_panel_io_handle_t io = ili9488->io;

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
    // In ESP-IDF v4.x the API used false for "on" and true for "off"
    // invert the logic to be consistent with IDF v5.x.
    on_off = !on_off;
#endif

    if (on_off)
    {
        esp_lcd_panel_io_tx_param(io, LCD_CMD_DISPON, NULL, 0);
    }
    else
    {
        esp_lcd_panel_io_tx_param(io, LCD_CMD_DISPOFF, NULL, 0);
    }

    // give time for the ILI9488 to recover after an on/off command
    vTaskDelay(pdMS_TO_TICKS(100));

    return ESP_OK;
}

esp_err_t esp_lcd_new_panel_ili9488(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config, esp_lcd_panel_handle_t *ret_panel)
{
    esp_err_t ret = ESP_OK;
    ili9488_panel_t *ili9488 = NULL;
    ESP_GOTO_ON_FALSE(io && panel_dev_config && ret_panel, ESP_ERR_INVALID_ARG, err, TAG, "invalid argument");
    ili9488 = (ili9488_panel_t *)(calloc(1, sizeof(ili9488_panel_t)));
    ESP_GOTO_ON_FALSE(ili9488, ESP_ERR_NO_MEM, err, TAG, "no mem for ili9488 panel");

    if (panel_dev_config->reset_gpio_num >= 0)
    {
        gpio_config_t cfg;
        memset(&cfg, 0, sizeof(gpio_config_t));
        esp_rom_gpio_pad_select_gpio(panel_dev_config->reset_gpio_num);
        cfg.pin_bit_mask = BIT64(panel_dev_config->reset_gpio_num);
        cfg.mode = GPIO_MODE_OUTPUT;
        ESP_GOTO_ON_ERROR(gpio_config(&cfg), err, TAG, "configure GPIO for RESET line failed");
    }

    switch (panel_dev_config->bits_per_pixel)
    {
        case 16:
            ili9488->color_mode = ILI9488_COLOR_MODE_16BIT;    
            ili9488->bits_per_pixel = 16;
            break;
        case 18:
        case 24:
            ili9488->color_mode = ILI9488_COLOR_MODE_18BIT;
            ili9488->bits_per_pixel = 24;
            break;
        default:
            ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "Unsupported pixel witdth");
            break;
    }

    switch (panel_dev_config->rgb_ele_order)
    {
        case ESP_LCD_COLOR_SPACE_RGB:
            ili9488->memory_access_control &= ~LCD_CMD_BGR_BIT;
            break;
        case ESP_LCD_COLOR_SPACE_BGR:
            ili9488->memory_access_control |= LCD_CMD_BGR_BIT;
            break;
        default:
            ESP_GOTO_ON_FALSE(false, ESP_ERR_INVALID_ARG, err, TAG, "Unsupported color mode!");
    }

    ili9488->io = io;
    ili9488->reset_gpio_num = panel_dev_config->reset_gpio_num;
    ili9488->reset_level = panel_dev_config->flags.reset_active_high;
    ili9488->base.del = panel_ili9488_del;
    ili9488->base.reset = panel_ili9488_reset;
    ili9488->base.init = panel_ili9488_init;
    ili9488->base.draw_bitmap = panel_ili9488_draw_bitmap;
    ili9488->base.invert_color = panel_ili9488_invert_color;
    ili9488->base.set_gap = panel_ili9488_set_gap;
    ili9488->base.mirror = panel_ili9488_mirror;
    ili9488->base.swap_xy = panel_ili9488_swap_xy;
    ili9488->base.disp_on_off = panel_ili9488_disp_on_off;
    *ret_panel = &(ili9488->base);
    ESP_LOGI(TAG, "new ili9488 panel @%p", ili9488);

    return ESP_OK;

err:
    if (ili9488)
    {
        if (panel_dev_config->reset_gpio_num >= 0)
        {
            gpio_reset_pin(panel_dev_config->reset_gpio_num);
        }
    }
    return ret;
}