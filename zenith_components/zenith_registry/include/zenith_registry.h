// zenith_registry.h

#pragma once

#include "freertos/FreeRTOS.h"
#include "esp_now.h"
#include "time.h"
#include "zenith_data.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ZENITH_MAC_ADDR_LEN 6

typedef uint8_t zenith_mac_address_t[ZENITH_MAC_ADDR_LEN];

// Forward declaration of opaque registry handle
typedef struct zenith_registry_s *zenith_registry_handle_t;
typedef float zenith_reading_datatype_t;  // Alias for now — could later become a union if needed.

// Registry sensor reading types
typedef enum zenith_sensor_type_e {
    ZENITH_SENSOR_TYPE_FIRST = 0,
    ZENITH_SENSOR_TYPE_TEMPERATURE,
    ZENITH_SENSOR_TYPE_HUMIDITY,
    ZENITH_SENSOR_TYPE_PRESSURE,
    ZENITH_SENSOR_TYPE_MAX
} zenith_sensor_type_t;


// sensor reading: sensor datapoint with timestamp
typedef struct zenith_reading_s {
    zenith_reading_datatype_t value; // Sensor value
    time_t timestamp;
} zenith_reading_t;


// Ringbuffer for storing sensor readings
#define ZENITH_RING_CAPACITY 32

typedef struct zenith_ringbuffer_s {
    zenith_sensor_type_t type;
    zenith_reading_t entries[ZENITH_RING_CAPACITY];
    size_t head;  // next write index
    size_t size;  // currently used slots
} zenith_ringbuffer_t;

typedef struct zenith_node_runtime_s {
    zenith_mac_address_t mac;
    size_t ring_count; // number of rings allocated
    zenith_ringbuffer_t *rings; // dynamically allocated array of rings
} zenith_node_runtime_t;


// Sensor datapoint
/* typedef struct zenith_datapoint_s {
    zenith_sensor_type_t sensor_type;
    zenith_reading_datatype_t value;
} zenith_datapoint_t; */

// Registry node information structure
typedef struct zenith_node_info_s {
    zenith_mac_address_t mac; // MAC address of the node
// Will add more information about the node, such as:
//    uint8_t model;   // Type of the node (e.g., sensor, actuator)
//    uint8_t version; // Firmware version of the node
//    char name[32];  // Name of the node (e.g., "Living Room Sensor")
//    char location[32]; // Location of the node (e.g., "Living Room")
} zenith_node_info_t;

// NVS header for the registry blob - we align by hand
typedef struct zenith_registry_nvs_header_s {
    uint8_t registry_version;
    uint8_t count;
} zenith_registry_nvs_header_t;

// NVS blob for the registry - we align by hand
typedef struct zenith_registry_nvs_blob_s {
    zenith_registry_nvs_header_t header;
    zenith_node_info_t nodes[];
} zenith_registry_nvs_blob_t;


// Event interface for the registry. Subscribers will be notified of changes to the registry and what ID these changes affect
typedef enum zenith_registry_event_e {
    ZENITH_REGISTRY_EVENT_NODE_ADDED,
    ZENITH_REGISTRY_EVENT_NODE_REMOVED,
    ZENITH_REGISTRY_EVENT_READING_UPDATED,
    ZENITH_REGISTRY_EVENT_NODE_UPDATED
} zenith_registry_event_t;

typedef void (*zenith_registry_callback_t)( zenith_registry_event_t event, const zenith_mac_address_t macv );

// Registry API

// Registry lifecycle
esp_err_t zenith_registry_new( zenith_registry_handle_t *out_handle );
esp_err_t zenith_registry_delete( zenith_registry_handle_t handle );
esp_err_t zenith_registry_register_callback( zenith_registry_handle_t handle, zenith_registry_callback_t callback );

// Node information management 
esp_err_t zenith_registry_store_node_info( zenith_registry_handle_t handle, const zenith_mac_address_t mac, const zenith_node_info_t *info );
esp_err_t zenith_registry_get_node_info( zenith_registry_handle_t handle, const zenith_mac_address_t mac, zenith_node_info_t *out_info );
esp_err_t zenith_registry_forget_node( zenith_registry_handle_t handle, const zenith_mac_address_t mac );

// Node enumeration
esp_err_t zenith_registry_get_node_count( zenith_registry_handle_t handle, size_t *out_count );
esp_err_t zenith_registry_get_all_node_macs( zenith_registry_handle_t handle, zenith_mac_address_t *out_macs, size_t *inout_count );
//esp_err_t zenith_registry_get_node_mac_by_index( zenith_registry_handle_t handle, size_t index, zenith_mac_address_t out_mac );

// Reading management
esp_err_t zenith_registry_store_datapoints( zenith_registry_handle_t handle, const zenith_mac_address_t mac, const zenith_datapoint_t *datapoints, size_t count );

esp_err_t zenith_registry_get_latest_readings( zenith_registry_handle_t handle, const zenith_mac_address_t mac, zenith_reading_t *out_readings, size_t *inout_count );
esp_err_t zenith_registry_get_history( zenith_registry_handle_t handle, const zenith_mac_address_t mac, zenith_sensor_type_t type, zenith_reading_t *out_history, size_t *inout_count );
esp_err_t zenith_registry_get_max_last_24h( zenith_registry_handle_t handle, const zenith_mac_address_t mac, zenith_sensor_type_t type, zenith_reading_t *out_max_reading );
esp_err_t zenith_registry_get_min_last_24h( zenith_registry_handle_t handle, const zenith_mac_address_t mac, zenith_sensor_type_t type, zenith_reading_t *out_min_reading );

#ifdef __cplusplus
}
#endif
