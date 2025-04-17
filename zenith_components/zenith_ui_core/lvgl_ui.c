#include "lvgl.h"

/*********
 * ui idea: Brukbarhet, ikke design
 * - Main screen
 *  Show sensors
 *   at the top - in a horizontal scrollable grid?
 * 
 *  Show Time and Date 
 * 
 *  Show Moon phase
 */


void test_lvgl_ui(lv_display_t *disp)
{
    lv_obj_t *scr = lv_display_get_screen_active(disp);
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
    lv_obj_t * lbl_temperature = lv_label_create(scr);
    lv_label_set_text(lbl_temperature, "-55,5°");
    lv_obj_set_size(lbl_temperature, 200, 50);
    lv_obj_set_style_text_font(lbl_temperature, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_align(lbl_temperature, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(lbl_temperature);
    //lv_obj_set_y(lbl_temperature, -20);

    // Create the humidity label
    lv_obj_t * lbl_humidity = lv_label_create(scr);
    lv_label_set_text(lbl_humidity, "100%");
    lv_obj_set_size(lbl_humidity, 100, 25);
    lv_obj_center(lbl_humidity);
    lv_obj_set_y(lbl_humidity, 80);
    lv_obj_set_style_text_font(lbl_humidity, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_align(lbl_humidity, LV_TEXT_ALIGN_CENTER, 0);

    
}