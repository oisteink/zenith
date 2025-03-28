#include <stdio.h>
#include "esp_err.h"
#include "zenith-blink.h"
#include "zenith-blink-private.h"

void init_zenith_blink(gpio_num_t gpio_pin) {
    // Define the led strip config
    led_strip_config_t strip_config = {
        .strip_gpio_num = gpio_pin,
        .led_model = LED_MODEL_WS2812,
        .max_leds = 1,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB,
        .flags.invert_out = false,
    };

    // I'm using the Remote Control Transceiver to drive it 
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = LED_STRIP_RMT_RES_HZ,
        .flags.with_dma = false, // My RMT does not support dma
    };

    // Gather them up into the strips configuration
    led_indicator_strips_config_t strips_config = {
        .led_strip_cfg = strip_config,
        .led_strip_driver = LED_STRIP_RMT,
        .led_strip_rmt_cfg = rmt_config,
    };

    const led_indicator_config_t config = {
        .mode = LED_STRIPS_MODE,
        .led_indicator_strips_config = &strips_config,
        .blink_lists = blink_mode,
        .blink_list_num = BLINK_MAX,
    };

    led_indicator = led_indicator_create(&config);
    assert(led_indicator);
};

void zenith_blink(led_indicator_blink_type_t blink_type){
    ESP_ERROR_CHECK(led_indicator_start(led_indicator, blink_type));
};

void zenith_blink_stop(led_indicator_blink_type_t blink_type){
    ESP_ERROR_CHECK(led_indicator_stop(led_indicator, blink_type));
    ESP_ERROR_CHECK(led_indicator_set_on_off(led_indicator, false));
};

void zenith_blink_off(void) {

}