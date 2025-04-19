    /*
 * SPDX-FileCopyrightText: 2021-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/lock.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"
#include "esp_check.h"
#include "esp_log.h"
#include "lvgl.h"
#include "esp_lcd_ili9488.h"


#include "esp_lcd_touch_xpt2046.h"
#include "lvgl_ui.h"
#include "zenith_ui_core.h"

static const char *TAG = "zenith_ui_core";

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Zenith Core UI
//

#define LCD_PIXEL_CLOCK_HZ     (20 * 1000 * 1000)


// LCD Panel
#define LCD_H_RES       (320)
#define LCD_V_RES       (480)
#define DISPLAY_COMMAND_BITS      (8)
#define DISPLAY_PARAMETER_BITS    (8)
#define DISPLAY_SPI_QUEUE_LEN    (10)
#define DISPLAY_BITS_PER_PIXEL   (24)
#define DISPLAY_BYTES_PER_PIXEL   (DISPLAY_BITS_PER_PIXEL >> 3)
#define DISPLAY_REFRESH_HZ  (40 * 1000 * 1000)

// LEDC Backlight
#define BACKLIGHT_LEDC_MODE  LEDC_LOW_SPEED_MODE
#define BACKLIGHT_LEDC_CHANNEL LEDC_CHANNEL_0
#define BACKLIGHT_LEDC_TIMER  LEDC_TIMER_1
#define BACKLIGHT_LEDC_TIMER_RESOLUTION LEDC_TIMER_10_BIT
#define BACKLIGHT_LEDC_FRQUENCY 5000

// Touch Panel

#define LVGL_DRAW_BUF_LINES    48 // number of display lines in each draw buffer
#define LVGL_TICK_PERIOD_MS    2
#define LVGL_TASK_STACK_SIZE   (4 * 1024)
#define LVGL_TASK_PRIORITY     2

// LVGL library is not thread-safe
static _lock_t lvgl_api_lock;

//extern void example_lvgl_demo_ui( lv_disp_t *disp );
//extern void app_lvgl_ui( lv_display_t *disp );

// TODO!!
// Create abstraction layer zenith_ui and implementation zenith_ui_core_touch
// Likely candidates for abstraction:
// del()
// set/fade brightness - zenith_ui_set_brightness(handle, brightness)
// init functions - so zenit_ui_init() runs them
//
// consider callbacks
// (esp_lcd_touch_handle_t tp, uint16_t *x, uint16_t *y, uint16_t *strength, uint8_t *point_num, uint8_t max_point_num)

static void zenith_ui_core_touch_apply_calibration( esp_lcd_touch_handle_t tp, uint16_t * x, uint16_t * y, uint16_t * strength, uint8_t * point_num, uint8_t max_point_num ) 
{
    for ( int i = 0; i < *point_num; i++ ) {
        // Clamp to calibrated min/max
        uint16_t x_raw = ( x[i] < 240 ) ? 240 : ( x[i] > 3800 ) ? 3800 : x[i];
        uint16_t y_raw = ( y[i] < 280 ) ? 280 : ( y[i] > 3800 ) ? 3800 : y[i];
        // Map to screen coordinates
        x[i] = ( x_raw - 240 ) * 319 / ( 3800 - 240 );
        y[i] = ( y_raw - 280 ) * 479 / ( 3800 - 280 );
    }
}

esp_err_t zenith_ui_core_init_lcd_backlight( int LCD_LED )
{
    ESP_LOGI( TAG, "Initializing backlight LEDC: GPIO[%d]", LCD_LED );
    const ledc_channel_config_t LCD_backlight_channel =
    {
        .gpio_num = LCD_LED,
        .speed_mode = BACKLIGHT_LEDC_MODE,
        .channel = BACKLIGHT_LEDC_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = BACKLIGHT_LEDC_TIMER,
        .duty = 0,
        .hpoint = 0,
        .flags = 
        {
            .output_invert = 0
        }
    };
    ESP_RETURN_ON_ERROR( ledc_channel_config( &LCD_backlight_channel ), TAG, "Error configuring LEDC channel" );

    const ledc_timer_config_t LCD_backlight_timer =
    {
        .speed_mode = BACKLIGHT_LEDC_MODE,
        .duty_resolution = BACKLIGHT_LEDC_TIMER_RESOLUTION,
        .timer_num = BACKLIGHT_LEDC_TIMER,
        .freq_hz = BACKLIGHT_LEDC_FRQUENCY,
        .clk_cfg = LEDC_AUTO_CLK
    };

    ESP_RETURN_ON_ERROR( 
        ledc_timer_config( &LCD_backlight_timer ),
        TAG, "Error configuring LEDC timer" 
    );

    ESP_RETURN_ON_ERROR(
        ledc_fade_func_install( 0 ),
        TAG, "Error installing LEDC fade intterupt"
    );

    return ESP_OK;
}

esp_err_t zenith_ui_core_fade_lcd_brightness( const int brightness_percentage, const uint32_t max_fade_time )
{
    uint32_t target_duty = ( 1023 * brightness_percentage ) / 100;
    ESP_RETURN_ON_ERROR(
        ledc_set_fade_time_and_start( BACKLIGHT_LEDC_MODE, BACKLIGHT_LEDC_CHANNEL, target_duty, max_fade_time, LEDC_FADE_NO_WAIT ),
        TAG, "Error fading lcd brightness"
    );

    return ESP_OK;
}

esp_err_t zenith_ui_core_set_lcd_brightness( int brightness_percentage )
{
    ESP_RETURN_ON_FALSE( ( brightness_percentage <= 100 && brightness_percentage >= 0 ), ESP_ERR_INVALID_ARG, TAG, "Invalid brightness: %d", brightness_percentage );
    if ( brightness_percentage > 100 )
    {
        brightness_percentage = 100;
    }    
    else if ( brightness_percentage < 0 )
    {
        brightness_percentage = 0;
    }
    ESP_LOGI( TAG, "Setting backlight to %d%%", brightness_percentage );

    // LEDC resolution set to 10bits; 100% = 1023
    uint32_t duty_cycle = ( 1023 * brightness_percentage ) / 100;
    ESP_RETURN_ON_ERROR( 
        ledc_set_duty_and_update( BACKLIGHT_LEDC_MODE, BACKLIGHT_LEDC_CHANNEL, duty_cycle, 0 ),
        TAG, "Error updating brightness" 
    );

    return ESP_OK;
}

static bool zenith_ui_core_notify_lvgl_flush_ready( esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t * edata, void * user_ctx )
{
    lv_display_t *disp = ( lv_display_t * )user_ctx;
    lv_display_flush_ready( disp );
    return false;
}

static void zenith_ui_core_lvgl_flush_cb( lv_display_t * disp, const lv_area_t *area, uint8_t * px_map )
{
    //zenith_ui_core_apply_lvgl_rotation(disp);
    esp_lcd_panel_handle_t panel_handle = lv_display_get_user_data( disp );
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;
    ESP_ERROR_CHECK(
        esp_lcd_panel_draw_bitmap( panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, px_map )
    );
}

static void zenith_ui_core_lvgl_touch_cb( lv_indev_t * indev, lv_indev_data_t * data )
{
    uint16_t touchpad_x[1] = {0};
    uint16_t touchpad_y[1] = {0};
    uint16_t strength[1] = {0};
    uint8_t touchpad_cnt = 0;
    esp_lcd_touch_handle_t touch_pad = lv_indev_get_user_data( indev );
    esp_lcd_touch_read_data(touch_pad);
    /* Get coordinates */
    bool touchpad_pressed = esp_lcd_touch_get_coordinates( touch_pad, touchpad_x, touchpad_y, strength, &touchpad_cnt, 1 );

    if ( touchpad_pressed && touchpad_cnt > 0 ) {
        data->point.x = touchpad_x[0];
        data->point.y = touchpad_y[0];
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static void zenith_ui_core_increase_lvgl_tick( void * arg )
{
    /* Tell LVGL how many milliseconds has elapsed */
    lv_tick_inc( LVGL_TICK_PERIOD_MS );
}

static void lvgl_task(
        void * arg
    )
{
    ESP_LOGI( TAG, "Starting LVGL task" );
    uint32_t time_till_next_ms = 0;
    uint32_t time_threshold_ms = 1000 / CONFIG_FREERTOS_HZ;
    while ( 1 ) {
        _lock_acquire( &lvgl_api_lock );
        time_till_next_ms = lv_timer_handler();
        _lock_release( &lvgl_api_lock );
        // in case of triggering a task watch dog time out
        time_till_next_ms = MAX(time_till_next_ms, time_threshold_ms);
        usleep(1000 * time_till_next_ms); //Maybe go for vTaskDelay here
    }
}

//SPI is device, so it should probably be initialized outside
esp_err_t zenith_ui_core_init_spi( spi_host_device_t spi_host, const int spi_sclk, const int spi_mosi, const int spi_miso )
{
    ESP_LOGI( TAG, "Initializing SPI%d_HOST: SCLK GPIO[%d] MOSI GPIO[%d] MISO GPIO[%d]", spi_host + 1, spi_sclk, spi_mosi, spi_miso );
    spi_bus_config_t buscfg = {
        .sclk_io_num = spi_sclk,
        .mosi_io_num = spi_mosi,
        .miso_io_num = spi_miso,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * LVGL_DRAW_BUF_LINES * DISPLAY_BYTES_PER_PIXEL, 
    };
    ESP_RETURN_ON_ERROR(
        spi_bus_initialize( spi_host, &buscfg, SPI_DMA_CH_AUTO ),
        TAG, "Error initializing SPI bus"
    );

    return ESP_OK;
}

esp_err_t zenith_ui_core_init_lcd_panel( spi_host_device_t spi_host, const int lcd_cs, const int lcd_dc, const int lcd_reset, esp_lcd_panel_io_handle_t *io_handle, esp_lcd_panel_handle_t *panel_handle )
{
    ESP_LOGI( TAG, "Installing lcd panel IO to SPI%d_HOST: CS GPIO[%d], DC GPIO[%d]", spi_host + 1, lcd_cs, lcd_dc );
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = lcd_dc,
        .cs_gpio_num = lcd_cs,
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = DISPLAY_COMMAND_BITS,
        .lcd_param_bits = DISPLAY_PARAMETER_BITS,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    // Attach the LCD to the SPI bus
    ESP_RETURN_ON_ERROR( 
        esp_lcd_new_panel_io_spi( ( esp_lcd_spi_bus_handle_t )spi_host, &io_config, io_handle ), 
        TAG, "Error adding lcd panel io to spi bus" 
    );

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = lcd_reset,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = DISPLAY_BYTES_PER_PIXEL * 8,
    };
    ESP_LOGI( TAG, "Installing ILI9341 panel driver: RESET GPIO[%d]", lcd_reset );
    ESP_RETURN_ON_ERROR(
        esp_lcd_new_panel_ili9488( *io_handle, &panel_config, panel_handle ),
        TAG, "Error installing lcd panel driver"
    );
    // Normal init sequence for esp_lcd
    ESP_RETURN_ON_ERROR(
        esp_lcd_panel_reset( *panel_handle ),
        TAG, "Error during lcd panel reset"
    );
    ESP_RETURN_ON_ERROR(
        esp_lcd_panel_init( *panel_handle ),
        TAG, "Error during lcd panel init"
    );
    ESP_RETURN_ON_ERROR(
        esp_lcd_panel_swap_xy( *panel_handle, false ), 
        TAG, "Error setting lcd panel swap_xy"
    );
    ESP_RETURN_ON_ERROR(
        esp_lcd_panel_mirror( *panel_handle, false, false ),
        TAG, "Error setting lcd panel mirror"
    );
    ESP_RETURN_ON_ERROR(
        esp_lcd_panel_disp_on_off( *panel_handle, true ),
        TAG, "Error turning lcd panel on"
    ); 

    return ESP_OK;
}

esp_err_t zenith_ui_core_init_touch_panel( spi_host_device_t spi_host, const int touch_cs, esp_lcd_panel_io_handle_t * touch_io_handle, esp_lcd_touch_handle_t * touch_panel_handle )
{
    ESP_LOGI( TAG, "Installing touch panel IO to SPI%d_HOST: CS GPIO[%d]", spi_host + 1, touch_cs );
    esp_lcd_panel_io_spi_config_t touch_io_config = ESP_LCD_TOUCH_IO_SPI_XPT2046_CONFIG( touch_cs );
    // Attach the TOUCH to the SPI bus
    ESP_RETURN_ON_ERROR(
        esp_lcd_new_panel_io_spi( ( esp_lcd_spi_bus_handle_t )spi_host, &touch_io_config, touch_io_handle ),
        TAG, "Error initializing touch panel IO on SPI bus %d with CS %d", spi_host, touch_cs
    );

    esp_lcd_touch_config_t touch_panel_config = {
        .x_max = LCD_H_RES,
        .y_max = LCD_V_RES,
        .rst_gpio_num = -1,
        .int_gpio_num = -1,
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 1, // It's opp/ned so we need to let esp_lcd_touch know this.
        },
        .process_coordinates = zenith_ui_core_touch_apply_calibration,
    };

    ESP_LOGI( TAG, "Initializing touch controller XPT2046" );
    ESP_RETURN_ON_ERROR(
        esp_lcd_touch_new_spi_xpt2046( *touch_io_handle, &touch_panel_config, touch_panel_handle ),
        TAG, "Error initializing touch panel controller"
    );
    
    return ESP_OK;
}

esp_err_t zenith_ui_core_init_lvgl( spi_host_device_t spi_host, const esp_lcd_panel_io_handle_t lcd_io_handle, const esp_lcd_panel_handle_t lcd_panel_handle, const esp_lcd_touch_handle_t touch_panel_handle, lv_display_t * *lvgl_display )
{
    ESP_LOGI( TAG, "Initializing LVGL");
    lv_init();
    ESP_LOGI( TAG, "Creating LVGL display");
    lv_display_t *display = lv_display_create( LCD_H_RES, LCD_V_RES );
    lv_display_set_rotation( display, LV_DISP_ROTATION_0 );

    ESP_LOGI( TAG, "Allocating DMA buffers on SPI%d_HOST for LVGL draw", spi_host + 1 );
    // it's recommended to choose the size of the draw buffer(s) to be at least 1/10 screen sized
    size_t draw_buffer_sz = LCD_H_RES * LVGL_DRAW_BUF_LINES * 3;//sizeof(lv_color16_t);

    void *buf1 = spi_bus_dma_memory_alloc( spi_host, draw_buffer_sz, 0 );
    ESP_RETURN_ON_FALSE( 
        buf1,
        ESP_ERR_NO_MEM,
        TAG, "Error creating first LVGL buffer"
    );

    void *buf2 = spi_bus_dma_memory_alloc( spi_host, draw_buffer_sz, 0 );
    ESP_RETURN_ON_FALSE(
        buf2,
        ESP_ERR_NO_MEM,
        TAG, "Error creating second LVGL buffer"
    );

    ESP_LOGI( TAG, "Attaching buffers to LVGL" );
    lv_display_set_buffers( display, buf1, buf2, draw_buffer_sz, LV_DISPLAY_RENDER_MODE_PARTIAL );
    // associate the lcd panel handle to the display
    lv_display_set_user_data( display, lcd_panel_handle) ;
    // set color depth
    lv_display_set_color_format( display, LV_COLOR_FORMAT_RGB888 );
    // set the callback which can copy the rendered image to an area of the display
    lv_display_set_flush_cb( display, zenith_ui_core_lvgl_flush_cb );

    ESP_LOGI( TAG, "Installing LVGL tick timer" );
    // Tick interface for LVGL (using esp_timer to generate 2ms periodic event)
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &zenith_ui_core_increase_lvgl_tick,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_RETURN_ON_ERROR(
        esp_timer_create( &lvgl_tick_timer_args, &lvgl_tick_timer ), 
        TAG, "Error creating lvgl tick timer"
    );
    ESP_RETURN_ON_ERROR(
        esp_timer_start_periodic( lvgl_tick_timer, LVGL_TICK_PERIOD_MS * 1000 ), 
        TAG, "Error staring lvgl tick timer"
    );

    ESP_LOGI( TAG, "Register IO panel event callback for LVGL flush ready notification" );
    const esp_lcd_panel_io_callbacks_t cbs = {
        .on_color_trans_done = zenith_ui_core_notify_lvgl_flush_ready,
    };
    ESP_RETURN_ON_ERROR(
        esp_lcd_panel_io_register_event_callbacks( lcd_io_handle, &cbs, display ), 
        TAG, "Error registring lcd io callback"
    );

    ESP_LOGI( TAG, "Creating LVGL input device (touch)" );
    static lv_indev_t *indev;
    indev = lv_indev_create();
    lv_indev_set_type( indev, LV_INDEV_TYPE_POINTER );
    ESP_LOGI( TAG, "Attaching LVGL input device to LVGL display" );
    lv_indev_set_display( indev, display );
    ESP_LOGI( TAG, "Setting LVGL input device user_data and callback" );
    lv_indev_set_user_data( indev, touch_panel_handle );
    lv_indev_set_read_cb( indev, zenith_ui_core_lvgl_touch_cb );

    ESP_LOGI( TAG, "Creating LVGL task" );
    ESP_RETURN_ON_FALSE(
        xTaskCreate( lvgl_task, "LVGL", LVGL_TASK_STACK_SIZE, NULL, LVGL_TASK_PRIORITY, NULL ) == pdTRUE,
        ESP_FAIL, 
        TAG, "Error starting lvgl task");

    *lvgl_display = display;

    return ESP_OK;
}

esp_err_t zenith_ui_del( zenith_ui_handle_t ui )
{
    // vi(vi? du og koden din?) trenger tilsvarende som for å lage - deinit_spi, deinit_lcd_backlight etc.
    /**********
    * LVGL - check docks for deinit
    * */
    // stop task
    // remove invdev
    // stop tick timer - inform lvlg that I do
    // Free buffers
    lv_deinit();
    /***********
     * esp_lcd_touch
     */
    esp_lcd_touch_del( ui->touch_panel_handle );
    esp_lcd_panel_io_del( ui->touch_io_handle );
    /************
     * esp_lcd_panel
     */
    esp_lcd_panel_del( ui->lcd_panel_handle );
    esp_lcd_panel_io_del( ui->lcd_io_handle );
    /************
     * spi
     */
    spi_bus_free( ui->config.spi_host );
    /***********
     * ledc
     */
    ledc_fade_func_uninstall();
    gpio_reset_pin( ui->config.lcd_backlight_pin );
    
    free( ui );

    return ESP_OK;
}

esp_err_t zenith_ui_new_core( const zenith_ui_config_t* config, zenith_ui_handle_t *ui )
{
    zenith_ui_handle_t handle = ( zenith_ui_handle_t ) calloc( 1, sizeof( zenith_ui_t ) );
    memcpy( &handle->config, config, sizeof(zenith_ui_config_t ) );
    ESP_RETURN_ON_ERROR(
        zenith_ui_core_init_spi( config->spi_host, config->sclk_pin, config->mosi_pin, config->miso_pin ),
        TAG, "Error initializing spi"
    );
    ESP_RETURN_ON_ERROR(
        zenith_ui_core_init_lcd_backlight( config->lcd_backlight_pin ),
        TAG, "Error initializing lcd panel backlight"
    );
    ESP_RETURN_ON_ERROR(
        zenith_ui_core_init_lcd_panel( config->spi_host, config->lcd_cs_pin, config->lcd_dc_pin, config->lcd_reset_pin, &handle->lcd_io_handle, &handle->lcd_panel_handle ),
        TAG, "Error initializing lcd panel"
    );
    ESP_RETURN_ON_ERROR(
        zenith_ui_core_init_touch_panel( config->spi_host, config->touch_cs_pin, &handle->touch_io_handle, &handle->touch_panel_handle ),
        TAG, "Error initializing touch panel"
    );
    ESP_RETURN_ON_ERROR(
        zenith_ui_core_init_lvgl( config->spi_host, handle->lcd_io_handle, handle->lcd_panel_handle, handle->touch_panel_handle, &handle->display ),
        TAG, "Error initializing LVGL"
    );

    handle->node_registry = config->node_registry;

    *ui = handle;

    return ESP_OK;
}

#ifndef MAC2STR
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#endif

esp_err_t zenith_ui_test( zenith_ui_handle_t ui)
{
    lv_obj_t *scr = lv_scr_act();
    
    // Create list
    lv_obj_t *list = lv_list_create( scr );
    lv_obj_set_size( list, LV_PCT( 100 ), LV_PCT( 100 ) );
    
    // Get registry data
    uint8_t count;
    zenith_registry_count( ui->node_registry, &count );
    
    // Populate list
    for(uint8_t i=0; i<count; i++) {
        uint8_t mac[6];
        zenith_registry_get_mac(ui->node_registry, i, mac);
        
        char mac_str[18];
        sprintf( mac_str, MACSTR, MAC2STR( mac ) );
        
        lv_list_add_btn( list, NULL, mac_str );
    }
    return ESP_OK;
}