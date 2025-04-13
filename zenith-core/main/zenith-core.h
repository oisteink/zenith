#pragma once

#include "freertos/FreeRTOS.h"

#define MAX_NODES 10

typedef struct node_data {
    uint8_t mac[ESP_NOW_ETH_ALEN];
    float temperature;
    float humidity;
    int64_t timestamp;
} node_data_t;

typedef struct nvs_node_data {
    uint8_t count;
    uint8_t macs[10][ESP_NOW_ETH_ALEN];
} nvs_node_data_t;


/**
 * ESP - LCD PINOUT      - wire
 * ----|-----------------|-----
 * 3v3 |  1 - VCC        | Red
 * GND |  2 - GND        | White
 *  18 |  3 - LCD CS     | Brown
 *  19 |  4 - LCD RESET  | Purple
 *  20 |  5 - LCD DC     | Black
 *   7 |  6 - LCD MOSI   | Green
 *   6 |  7 - LCD SCLK   | Blue
 *  21 |  8 - LED BACKL  | Yellow
 *  NC |  9 - LED MISO   | NC
 *   6 | 10 - Touch SCLK | Blue
 *  22 | 11 - Touch CS   | Brown
 *   7 | 12 - Touch MOSI | Green
 *   2 | 13 - Touch MISO | Orange
 *  NC | 14 - Touch INT  | NC
 */