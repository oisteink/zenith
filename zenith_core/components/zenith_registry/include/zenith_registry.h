#pragma once
#include "freertos/FreeRTOS.h"
#include "esp_now.h"

#define ZENITH_REGISTRY_MAX_NODES 10
#define ZENITH_REGISTRY_MAX_NAME_LEN 10
#define ZENITH_REGISTRY_VERSION ( uint8_t ) 1
#define ZENITH_REGISTRY_NODE_KEY "paired_nodes"
#define ZENITH_REGISTRY_NAMESPACE "ZenithR"

typedef struct zenith_registry_s zenith_registry_t;
typedef zenith_registry_t* zenith_registry_handle_t;

typedef struct {
    uint8_t mac[ESP_NOW_ETH_ALEN];
    char name[ZENITH_REGISTRY_MAX_NAME_LEN];
} zenith_node_t;

struct zenith_registry_s {
    uint8_t registry_version;
    uint8_t count;
    zenith_node_t nodes[ZENITH_REGISTRY_MAX_NODES];
};

esp_err_t zenith_registry_create( zenith_registry_handle_t* handle );
esp_err_t zenith_registry_init( zenith_registry_handle_t handle );
esp_err_t zenith_registry_deinit( zenith_registry_handle_t handle );
esp_err_t zenith_registry_add( zenith_registry_handle_t handle, const zenith_node_t node );
esp_err_t zenith_registry_remove( zenith_registry_handle_t handle, const uint8_t index );
esp_err_t zenith_registry_get( zenith_registry_handle_t handle, const uint8_t index, zenith_node_t* node );
esp_err_t zenith_registry_update( zenith_registry_handle_t handle, const uint8_t index, zenith_node_t node );
esp_err_t zenith_registry_count( zenith_registry_handle_t handle, uint8_t* count );

int8_t zenith_registry_index_of_mac( zenith_registry_handle_t handle, const uint8_t mac[ESP_NOW_ETH_ALEN] );

// why did i make this? Probably outdated and should be removed.
esp_err_t zenith_registry_get_mac( zenith_registry_handle_t handle, uint8_t index, uint8_t mac[ESP_NOW_ETH_ALEN] );