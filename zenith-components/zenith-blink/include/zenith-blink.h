#pragma once

#include "led_indicator.h"

typedef enum led_indicator_blink_type {
    BLINK_DATA_RECEIVE = 0,
    BLINK_PAIRING,
    BLINK_PAIRING_COMPLETE,
    BLINK_MAX
} led_indicator_blink_type_t;

void init_led(gpio_num_t gpio_pin);
void zenith_blink(led_indicator_blink_type_t blink_type);
