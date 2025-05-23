#pragma once

#include "led_indicator.h"
#include "zenith_blink.h"

#define LED_STRIP_RMT_RES_HZ (10 * 1000 * 1000) // 10 MHz

#ifdef ZBDEBUG
static char * TAG = "zenith_blink";
#endif

static led_indicator_handle_t led_indicator = NULL;

const blink_step_t blink_data_send[] = {
    {LED_BLINK_RGB, SET_RGB(0, 50, 50), 0},
    {LED_BLINK_HOLD, LED_STATE_ON, 10},
    {LED_BLINK_HOLD, LED_STATE_OFF, 0},
    {LED_BLINK_STOP, 0, 0},
};

const blink_step_t blink_data_receive[] = {
    {LED_BLINK_RGB, SET_RGB(50, 50, 0), 0},
    {LED_BLINK_BREATHE, LED_STATE_25_PERCENT, 250},
    {LED_BLINK_BREATHE, LED_STATE_OFF, 250},
    {LED_BLINK_STOP, 0, 0},
};

const blink_step_t blink_data_unknown_peer[] = {
    {LED_BLINK_RGB, SET_RGB(50, 0, 0), 0},
    {LED_BLINK_BREATHE, LED_STATE_25_PERCENT, 250},
    {LED_BLINK_BREATHE, LED_STATE_OFF, 250},
    {LED_BLINK_STOP, 0, 0},
};

const blink_step_t blink_pairing[] = {
    {LED_BLINK_RGB, SET_RGB(50, 0, 50), 0},
    {LED_BLINK_HOLD, LED_STATE_25_PERCENT, 10},
    {LED_BLINK_HOLD, LED_STATE_OFF, 0},
    {LED_BLINK_STOP, 0, 0},
};

const blink_step_t blink_pairing_complete[] = {
    {LED_BLINK_RGB, SET_RGB(50, 0, 50), 0},
    {LED_BLINK_BREATHE, LED_STATE_25_PERCENT, 150},
    {LED_BLINK_BREATHE, LED_STATE_OFF, 100},
    {LED_BLINK_BREATHE, LED_STATE_25_PERCENT, 150},
    {LED_BLINK_BREATHE, LED_STATE_OFF, 100},
    {LED_BLINK_STOP, 0, 0},
};

blink_step_t const *blink_mode[] = {
    [BLINK_DATA_SEND] = blink_data_send,
    [BLINK_DATA_RECEIVE] = blink_data_receive,
    [BLINK_DATA_UNKNOWN_PEER] = blink_data_unknown_peer,
    [BLINK_PAIRING] = blink_pairing,
    [BLINK_PAIRING_COMPLETE] = blink_pairing_complete,
    [BLINK_MAX] = NULL,
};
