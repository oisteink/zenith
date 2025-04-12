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


static lv_obj_t * btn;

static void set_angle(void * obj, int32_t v)
{
    lv_arc_set_value(obj, v);
}

void example_lvgl_demo_ui(lv_display_t *disp)
{
    lv_obj_t *scr = lv_display_get_screen_active(disp);

    btn = lv_button_create(scr);
    lv_obj_t * lbl = lv_label_create(btn);
    lv_label_set_text_static(lbl, LV_SYMBOL_REFRESH" ROTATE");
    lv_obj_set_pos(btn, 100, 100);
    //lv_obj_align(btn, LV_ALIGN_BOTTOM_LEFT, 30, -30);
    /*Button event*/
    //lv_obj_add_event_cb(btn, btn_cb, LV_EVENT_CLICKED, disp);

    /*Create an Arc*/
    lv_obj_t * arc = lv_arc_create(scr);
    lv_arc_set_rotation(arc, 270);
    lv_arc_set_bg_angles(arc, 0, 360);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);   /*Be sure the knob is not displayed*/
    lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);  /*To not allow adjusting by click*/
    lv_obj_center(arc);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, arc);
    lv_anim_set_exec_cb(&a, set_angle);
    lv_anim_set_duration(&a, 1000);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);    /*Just for the demo*/
    lv_anim_set_repeat_delay(&a, 500);
    lv_anim_set_values(&a, 0, 100);
    lv_anim_start(&a);
}

void app_lvgl_ui(lv_display_t *disp)
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
    lv_obj_set_style_text_font(lbl_temperature, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(lbl_temperature, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(lbl_temperature);
    //lv_obj_set_y(lbl_temperature, -20);

    // Create the humidity label
    lv_obj_t * lbl_humidity = lv_label_create(scr);
    lv_label_set_text(lbl_humidity, "100%");
    lv_obj_set_size(lbl_humidity, 100, 25);
    lv_obj_center(lbl_humidity);
    lv_obj_set_y(lbl_humidity, 80);
    lv_obj_set_style_text_font(lbl_humidity, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(lbl_humidity, LV_TEXT_ALIGN_CENTER, 0);

    
}

static void drag_event_handler(lv_event_t * e)
{
    lv_obj_t * obj = lv_event_get_target(e);

    lv_indev_t * indev = lv_indev_active();
    if(indev == NULL)  return;

    lv_point_t vect;
    lv_indev_get_vect(indev, &vect);

    int32_t x = lv_obj_get_x_aligned(obj) + vect.x;
    int32_t y = lv_obj_get_y_aligned(obj) + vect.y;
    lv_obj_set_pos(obj, x, y);
}

/**
 * Make an object draggable.
 */
void lv_example_obj_2(void)
{
    lv_obj_t * obj;
    obj = lv_obj_create(lv_screen_active());
    lv_obj_set_size(obj, 150, 100);
    lv_obj_add_event_cb(obj, drag_event_handler, LV_EVENT_PRESSING, NULL);

    lv_obj_t * label = lv_label_create(obj);
    lv_label_set_text(label, "Drag me");
    lv_obj_center(label);

}