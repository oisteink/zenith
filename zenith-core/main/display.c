/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

 #include <stdio.h>
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
 #include "esp_err.h"
 #include "lvgl.h"
 
 
 #include "esp_lcd_gc9a01.h"
 
 
 // Using SPI2
 #define LCD_HOST  SPI2_HOST
 
 // Display configuration
 #define LCD_PIXEL_CLOCK_HZ     (20 * 1000 * 1000)
 #define PIN_NUM_SCLK           19 //23
 #define PIN_NUM_MOSI           20 //22
 #define PIN_NUM_LCD_DC         21
 #define PIN_NUM_LCD_CS         22 //20
 #define PIN_NUM_LCD_RST        23 //19
 
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
 
 // LVGL library is not thread-safe, this example will call LVGL APIs from different tasks, so use a mutex to protect it
 static _lock_t lvgl_api_lock;
 lv_obj_t * lbl_temperature = NULL;
 lv_obj_t * lbl_humidity = NULL;

 /// @brief Updates the display with new values for temperature and humidity
 /// @param temperature New temperature value
 /// @param humidity New humidity value
 void display_update_values(float temperature, float humidity) {
    char humidity_text[16], temperature_text[16];
    sprintf(humidity_text, "%.0f%%", humidity);
    sprintf(temperature_text, "%.1f°", temperature);
    // Lock the mutex due to the LVGL APIs are not thread-safe
    _lock_acquire(&lvgl_api_lock);
    lv_label_set_text(lbl_temperature, temperature_text);
    lv_label_set_text(lbl_humidity, humidity_text);
    _lock_release(&lvgl_api_lock);
}

 /// @brief Defines the LVGL User Interface
 /// @param disp LVGL Display to use
 void lvgl_ui(lv_display_t *disp)
{
    lv_obj_t *scr = lv_display_get_screen_active(disp);

/*     // Create an Arc
    lv_obj_t * arc = lv_arc_create(scr);
    lv_arc_set_rotation(arc, 90);
    lv_arc_set_bg_angles(arc, 60, 300);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);    // Be sure the knob is not displayed
    lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);  // To not allow adjusting by click
    lv_obj_set_size(arc, 234, 234);
    lv_obj_center(arc);

    // Animate it for test purposes. Maybe I need two arcs - one for negative and one for positive
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, arc);
    lv_anim_set_exec_cb(&a, set_angle);
    lv_anim_set_duration(&a, 1000);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);    
    lv_anim_set_repeat_delay(&a, 500);
    lv_anim_set_values(&a, 0, 100);
    lv_anim_start(&a);
 */

    // Create a scale along the edge
     lv_obj_t * scale_temperature = lv_scale_create(scr);
    lv_scale_set_mode(scale_temperature, LV_SCALE_MODE_ROUND_INNER);
    lv_scale_set_angle_range(scale_temperature, 240);
    lv_scale_set_range(scale_temperature, -30, 40);
    lv_scale_set_draw_ticks_on_top(scale_temperature, true);
    lv_scale_set_total_tick_count(scale_temperature, 71);
    lv_scale_set_major_tick_every(scale_temperature, 10);
    lv_obj_set_size(scale_temperature, 234, 234);
    lv_scale_set_rotation(scale_temperature, 150);
    lv_obj_center(scale_temperature);

    // Create the temperature label
    lbl_temperature = lv_label_create(scr);
    //lv_label_set_text(lbl_temperature, "-55,5°");
    lv_obj_set_size(lbl_temperature, 200, 50);
    lv_obj_set_style_text_font(lbl_temperature, &lv_font_montserrat_42, 0);
    lv_obj_set_style_text_align(lbl_temperature, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(lbl_temperature);
    //lv_obj_set_y(lbl_temperature, -20);

    // Create the humidity label
    lbl_humidity = lv_label_create(scr);
    //lv_label_set_text(lbl_humidity, "100%");
    lv_obj_set_size(lbl_humidity, 100, 25);
    lv_obj_center(lbl_humidity);
    lv_obj_set_y(lbl_humidity, 80);
    lv_obj_set_style_text_font(lbl_humidity, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_align(lbl_humidity, LV_TEXT_ALIGN_CENTER, 0);
}
 
 /// @brief Callback invoked when color data transfer has finished 
 /// @param panel_io LCD panel IO handle
 /// @param edata LCD panel IO event data
 /// @param user_ctx Callback invoked when color data transfer has finished
 /// @return always returns false
 static bool notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
 {
     lv_display_t *disp = (lv_display_t *)user_ctx;
     lv_display_flush_ready(disp);
     return false;
 }
 
 // Not sure this is needed outside of setting rotation. It's called from flush, so each time we are updating screen. Seems like a waste to me...
 /* Rotate display and touch, when rotated screen in LVGL. Called when driver parameters are updated. */
 /* static void lvgl_port_update_callback(lv_display_t *disp)
 {
     esp_lcd_panel_handle_t panel_handle = lv_display_get_user_data(disp);
     lv_display_rotation_t rotation = lv_display_get_rotation(disp);
 
     switch (rotation) {
     case LV_DISPLAY_ROTATION_0:
         // Rotate LCD display
         esp_lcd_panel_swap_xy(panel_handle, false);
         esp_lcd_panel_mirror(panel_handle, true, false);
         break;
     case LV_DISPLAY_ROTATION_90:
         // Rotate LCD display
         esp_lcd_panel_swap_xy(panel_handle, true);
         esp_lcd_panel_mirror(panel_handle, true, true);
         break;
     case LV_DISPLAY_ROTATION_180:
         // Rotate LCD display
         esp_lcd_panel_swap_xy(panel_handle, false);
         esp_lcd_panel_mirror(panel_handle, false, true);
         break;
     case LV_DISPLAY_ROTATION_270:
         // Rotate LCD display
         esp_lcd_panel_swap_xy(panel_handle, true);
         esp_lcd_panel_mirror(panel_handle, false, false);
         break;
     }
 } */
 
 /// @brief Callback which will be called to copy the rendered image to the display (buffer is flushed)
 /// @param disp Display to paint on
 /// @param area Area to paint
 /// @param px_map Buffer containing the data to draw
 static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
 {
     //lvgl_port_update_callback(disp);
     esp_lcd_panel_handle_t panel_handle = lv_display_get_user_data(disp);
     int offsetx1 = area->x1;
     int offsetx2 = area->x2;
     int offsety1 = area->y1;
     int offsety2 = area->y2;
     // because SPI LCD is big-endian, we need to swap the RGB bytes order
     lv_draw_sw_rgb565_swap(px_map, (offsetx2 + 1 - offsetx1) * (offsety2 + 1 - offsety1));
     // copy a buffer's content to a specific area of the display
     esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, px_map);
 }
 
 static void increase_lvgl_tick(void *arg)
 {
     /* Tell LVGL how many milliseconds has elapsed */
     lv_tick_inc(LVGL_TICK_PERIOD_MS);
 }
 
 static void lvgl_port_task(void *arg)
 {
     uint32_t time_till_next_ms = 0;
     uint32_t time_threshold_ms = 1000 / CONFIG_FREERTOS_HZ;
     while (1) {
         _lock_acquire(&lvgl_api_lock);
         time_till_next_ms = lv_timer_handler();
         _lock_release(&lvgl_api_lock);
         // in case of triggering a task watch dog time out
         time_till_next_ms = MAX(time_till_next_ms, time_threshold_ms);
         vTaskDelay(pdMS_TO_TICKS(time_till_next_ms));
         //usleep(1000 * time_till_next_ms);
     }
 }
 
 esp_err_t display_init(void)
 {
     spi_bus_config_t buscfg = {
         .sclk_io_num = PIN_NUM_SCLK,
         .mosi_io_num = PIN_NUM_MOSI,
         .miso_io_num = -1,
         .quadwp_io_num = -1,
         .quadhd_io_num = -1,
         .max_transfer_sz = LCD_H_RES * 80 * sizeof(uint16_t),
     };
     ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));
 
     esp_lcd_panel_io_handle_t io_handle = NULL;
     esp_lcd_panel_io_spi_config_t io_config = {
         .dc_gpio_num = PIN_NUM_LCD_DC,
         .cs_gpio_num = PIN_NUM_LCD_CS,
         .pclk_hz = LCD_PIXEL_CLOCK_HZ,
         .lcd_cmd_bits = LCD_CMD_BITS,
         .lcd_param_bits = LCD_PARAM_BITS,
         .spi_mode = 0,
         .trans_queue_depth = 10,
     };
     // Attach the LCD to the SPI bus
     ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));
 
     esp_lcd_panel_handle_t panel_handle = NULL;
     esp_lcd_panel_dev_config_t panel_config = {
         .reset_gpio_num = PIN_NUM_LCD_RST,
         .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
         .bits_per_pixel = 16,
     };
     ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(io_handle, &panel_config, &panel_handle));
 
     ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
     ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
 
     ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));
 
     ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, true, false));
 
     // user can flush pre-defined pattern to the screen before we turn on the screen or backlight
     ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
 
     lv_init();
 
     // create a lvgl display
     lv_display_t *display = lv_display_create(LCD_H_RES, LCD_V_RES);
 
     // alloc draw buffers used by LVGL
     // it's recommended to choose the size of the draw buffer(s) to be at least 1/10 screen sized
     size_t draw_buffer_sz = LCD_H_RES * LVGL_DRAW_BUF_LINES * sizeof(lv_color16_t);
 
     void *buf1 = spi_bus_dma_memory_alloc(LCD_HOST, draw_buffer_sz, 0);
     assert(buf1);
     void *buf2 = spi_bus_dma_memory_alloc(LCD_HOST, draw_buffer_sz, 0);
     assert(buf2);
     // initialize LVGL draw buffers
     lv_display_set_buffers(display, buf1, buf2, draw_buffer_sz, LV_DISPLAY_RENDER_MODE_PARTIAL);
     // associate the mipi panel handle to the display
     lv_display_set_user_data(display, panel_handle);
     // set color depth
     lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
     // set the callback which can copy the rendered image to an area of the display
     lv_display_set_flush_cb(display, lvgl_flush_cb);
 
     // Tick interface for LVGL (using esp_timer to generate 2ms periodic event)
     const esp_timer_create_args_t lvgl_tick_timer_args = {
         .callback = &increase_lvgl_tick,
         .name = "lvgl_tick"
     };
     esp_timer_handle_t lvgl_tick_timer = NULL;
     ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
     ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, LVGL_TICK_PERIOD_MS * 1000)); //parameter in microseconds
 
     const esp_lcd_panel_io_callbacks_t cbs = {
         .on_color_trans_done = notify_lvgl_flush_ready,
     };
     /* Register done callback */
     ESP_ERROR_CHECK(esp_lcd_panel_io_register_event_callbacks(io_handle, &cbs, display));
 
 
     xTaskCreate(lvgl_port_task, "LVGL", LVGL_TASK_STACK_SIZE, NULL, LVGL_TASK_PRIORITY, NULL);
 
     // Lock the mutex due to the LVGL APIs are not thread-safe
     _lock_acquire(&lvgl_api_lock);
     lvgl_ui(display);
     _lock_release(&lvgl_api_lock);
     return ESP_OK;
 }
 