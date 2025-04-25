// zenith_registry.h
#pragma once
#include "freertos/FreeRTOS.h"
#include "esp_now.h"
#include "time.h"
#include "zenith_data.h"

#define ZENITH_REGISTRY_MAX_NODES 10
#define ZENITH_REGISTRY_MAX_NAME_LEN 10
#define ZENITH_REGISTRY_VERSION ( uint8_t ) 1
#define ZENITH_REGISTRY_NODE_KEY "paired_nodes"
#define ZENITH_REGISTRY_NAMESPACE "ZenithR"

typedef struct zenith_registry_s zenith_registry_t;
typedef zenith_registry_t* zenith_registry_handle_t;

typedef struct __attribute__((packed)) zenith_node_info_s {
    uint8_t mac[ESP_NOW_ETH_ALEN];
//    char name[ZENITH_REGISTRY_MAX_NAME_LEN];
//  fw version node is running?
//  Other shit like options maybe. Like if some sensors are to be supressed/ignored.
} zenith_node_info_t;

typedef struct __attribute__((packed)) zenith_registry_nvs_header_s {
    uint8_t registry_version;
    uint8_t count;
} zenith_registry_nvs_header_t;

typedef struct  __attribute__((packed)) zenith_registry_nvs_blob_s {
    zenith_registry_nvs_header_t header;
    zenith_node_info_t nodes[];
} zenith_registry_nvs_blob_t;

typedef struct zenith_node_data_s {
    time_t timestamp;                    
    zenith_datapoints_handle_t datapoints_handle;
} zenith_node_data_t;
typedef zenith_node_data_t* zenith_node_data_handle_t;

typedef struct zenith_node_s {
    zenith_node_info_t info;
    zenith_node_data_t data;
} zenith_node_t;
typedef zenith_node_t* zenith_node_handle_t;

struct zenith_registry_s {
    uint8_t registry_version;
    uint8_t count;
    zenith_node_handle_t nodes[ZENITH_REGISTRY_MAX_NODES];
};

/// @brief Creates a new zenith registry handle and initializes it
/// @param handle output handle
/// @return ESP_OK on success, otherwise error value
esp_err_t zenith_registry_create( zenith_registry_handle_t* handle );

/// @brief Deinitializes the zenith registry
/// @param handle zenith registry handle
/// @return ESP_OK on success, otherwise error value
esp_err_t zenith_registry_dispose( zenith_registry_handle_t handle );

esp_err_t zenith_registry_set_node_data( zenith_registry_handle_t handle, const uint8_t mac[ESP_NOW_ETH_ALEN], const zenith_node_data_handle_t data );
esp_err_t zenith_registry_get_node_data( zenith_registry_handle_t handle, const uint8_t mac[ESP_NOW_ETH_ALEN], zenith_node_data_handle_t *data );
esp_err_t zenith_registry_set_node_info( zenith_registry_handle_t handle, const uint8_t mac[ESP_NOW_ETH_ALEN], const zenith_node_info_t *info );
esp_err_t zenith_registry_get_node_info( zenith_registry_handle_t handle, const uint8_t mac[ESP_NOW_ETH_ALEN], zenith_node_info_t *info );

/// @brief Gets the data for a node from the registry by index. Creates a new node if it does not exist.
/// @param handle zenith registry handle
/// @param index index of the node to get data for
/// @param data output zenith node data struct
/// @return ESP_OK on success, otherwise error value
esp_err_t zenith_registry_retrieve_node_by_index( zenith_registry_handle_t handle, const uint8_t index, zenith_node_handle_t *data );

/// @brief Gets the data for a node from the registry by MAC address. Creates a new node if it does not exist.
/// @param handle zenith registry handle
/// @param mac MAC address of the node to get data for
/// @param data output zenith node data struct
/// @return ESP_OK on success, otherwise error value 
esp_err_t zenith_registry_retrieve_node( zenith_registry_handle_t handle, const uint8_t mac[ESP_NOW_ETH_ALEN], zenith_node_handle_t *data );

/// @brief Removes a node from the registry
/// @param handle  zenith registry handle
/// @param index index of the node to remove
/// @return ESP_OK on success, otherwise error value
esp_err_t zenith_registry_forget_node( zenith_registry_handle_t handle, const uint8_t index );

/// @brief Returns the number of nodes in the registry
/// @param handle zenith registry handle
/// @param count output number of nodes in the registry
/// @return ESP_OK on success, otherwise error value
size_t zenith_registry_count( zenith_registry_handle_t handle);

/// @brief Returns the index of a node in the registry by its MAC address
/// @param handle zenith registry handle
/// @param mac MAC address of the node
/// @return index of the node in the registry, or -1 if not found
size_t zenith_registry_index_of_mac( zenith_registry_handle_t handle, const uint8_t mac[ESP_NOW_ETH_ALEN] );