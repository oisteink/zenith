#pragma once

//#define ZBDEBUG 1

#include "esp_err.h"
#include "led_indicator.h"


typedef enum led_indicator_blink_type {
    BLINK_DATA_SEND = 0,
    BLINK_DATA_RECEIVE,
    BLINK_DATA_UNKNOWN_PEER,
    BLINK_PAIRING,
    BLINK_PAIRING_COMPLETE,
    BLINK_MAX
} led_indicator_blink_type_t;

esp_err_t init_zenith_blink(gpio_num_t gpio_pin);
esp_err_t zenith_blink(led_indicator_blink_type_t blink_type);
esp_err_t zenith_blink_stop(led_indicator_blink_type_t blink_type);
