#pragma once

#include "freertos/FreeRTOS.h"
#include "esp_now.h"

/* Structures for espnow data */
typedef float zenith_now_sensor_datatype_t;

typedef struct zenith_now_sensor_data {
    zenith_now_sensor_datatype_t temperature;
    zenith_now_sensor_datatype_t humidity;
} zenith_now_sensor_data_t;

typedef enum zenith_now_packet_type {
    ZENITH_PACKET_PAIRING = 1, // Pairing Request
    ZENITH_PACKET_DATA,        // Sensor data
    ZENITH_PACKET_ACK,          // Acknowledgment for other packet types
    ZENITH_PACKET_MAX
} zenith_now_packet_type_t;

typedef struct zenith_now_packet {
    uint8_t type; // Packet type (1 byte)
    union {
        zenith_now_sensor_data_t sensor_data; // For datapakker
        uint8_t ack_packet_type;   // For ackpakker
    };
} zenith_now_packet_t;

/* Structures for xQueue handling */

typedef enum zenith_now_event_type {
    SEND_EVENT = 1,
    RECEIVE_EVENT = 2
} zenith_now_event_type_t;

typedef struct zenith_now_send_event {
    uint8_t dest_mac[ESP_NOW_ETH_ALEN];
    esp_now_send_status_t status;
} zenith_now_send_event_t;

typedef struct zenith_now_receive_event {
    uint8_t source_mac[ESP_NOW_ETH_ALEN];
    zenith_now_packet_t data_packet;
} zenith_now_receive_event_t;

typedef struct zenith_event {
    zenith_now_event_type_t type;
    union {
        zenith_now_send_event_t send;
        zenith_now_receive_event_t receive;
    };
} zenith_now_event_t;

/* Callbacks */
typedef void (*zenith_rx_cb_t)(const uint8_t *mac, const zenith_now_packet_t *packet); //maybe change this to one callbeck for peering request and one for data 
typedef void (*zenith_tx_cb_t)(const uint8_t *mac, esp_now_send_status_t status);

/* Macros */

#define ZENITH_WIFI_CHANNEL 1
#define PAIRING_ACK_BIT BIT0
#define DATA_ACK_BIT BIT1
#define SENSOR_DATA_SIZE (sizeof(zenith_now_sensor_datatype_t))

extern QueueHandle_t zenith_now_event_queue;
extern EventGroupHandle_t zenith_now_event_group;

esp_err_t configure_zenith_now(void);
void zenith_now_set_rx_cb(zenith_rx_cb_t rx_cb);
void zenith_now_set_tx_cb(zenith_tx_cb_t tx_cb);
esp_err_t zenith_now_add_peer(const uint8_t * mac);
esp_err_t zenith_now_remove_peer(const uint8_t *mac);
esp_err_t zenith_now_send_packet(const uint8_t *peer_addr, const zenith_now_packet_t data_packet);
esp_err_t zenith_now_send_ack(const uint8_t *peer_addr, zenith_now_packet_type_t packet_type);
esp_err_t zenith_now_wait_for_ack(zenith_now_packet_type_t packet_type, uint32_t wait_ms);
const char * zenith_now_packet_type_to_str(zenith_now_packet_type_t packet_type);