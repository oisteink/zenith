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
    uint8_t macs[MAX_NODES][ESP_NOW_ETH_ALEN];
} nvs_node_data_t;
