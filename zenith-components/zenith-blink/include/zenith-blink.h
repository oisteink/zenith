#pragma once

#include "led_indicator.h"

typedef enum led_indicator_blink_type {
    BLINK_DATA_SEND = 0,
    BLINK_DATA_RECEIVE,
    BLINK_PAIRING,
    BLINK_PAIRING_COMPLETE,
    BLINK_MAX
} led_indicator_blink_type_t;

void init_zenith_blink(gpio_num_t gpio_pin);
void zenith_blink(led_indicator_blink_type_t blink_type);
void zenith_blink_stop(led_indicator_blink_type_t blink_type);
