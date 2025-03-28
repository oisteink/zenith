#pragma once

#include "freertos/FreeRTOS.h"
#include "esp_now.h"

/* Structures for espnow data */
typedef float sensor_datatype_t;

typedef struct sensor_data {
    sensor_datatype_t temperature;
    sensor_datatype_t humidity;
} zenith_sensor_data_t;

typedef enum zenith_packet_type {
    PACKET_PAIRING = 1, // Pairing Request
    PACKET_DATA,        // Sensor data
    PACKET_PING,        // Ping packet
    PACKET_ACK          // Acknowledgment for other packet types
} zenith_packet_type_t;

typedef struct zenith_packet {
    uint8_t type; // Packet type (1 byte)
    union {
        zenith_sensor_data_t sensor_data; // For datapakker
        uint8_t ack_packet_type;   // For ackpakker
    };
} zenith_packet_t;

/* Structures for xQueue handling */

typedef enum zenith_event_type {
    SEND_EVENT = 1,
    RECEIVE_EVENT = 2
} zenith_event_type_t;

typedef struct zenith_send_event {
    uint8_t dest_mac[ESP_NOW_ETH_ALEN];
    esp_now_send_status_t status;
} zenith_send_event_t;

typedef struct zenith_receive_event {
    uint8_t source_mac[ESP_NOW_ETH_ALEN];
    zenith_packet_t data_packet;
} zenith_receive_event_t;

typedef struct zenith_event {
    zenith_event_type_t type;
    union {
        zenith_send_event_t send;
        zenith_receive_event_t receive;
    };
} zenith_event_t;

/* Callbacks */
typedef void (*zenith_rx_cb_t)(const uint8_t *mac, const zenith_packet_t *packet); //maybe change this to one callbeck for peering request and one for data 
typedef void (*zenith_tx_cb_t)(const uint8_t *mac, esp_now_send_status_t status);

/* Macros */

#define ZENITH_WIFI_CHANNEL 1
#define PAIRING_ACK_BIT BIT0
#define DATA_ACK_BIT BIT1
#define PING_ACK_BIT BIT3
#define SENSOR_DATA_SIZE (sizeof(sensor_datatype_t))

extern QueueHandle_t zenith_event_queue;
extern EventGroupHandle_t zenith_event_group;

void configure_zenith(zenith_rx_cb_t rx_cb, zenith_tx_cb_t tx_cb);
void zenith_send_data(const uint8_t *peer_addr, const zenith_packet_t data_packet);
bool zenith_wait_for_ack(zenith_packet_type_t packet_type, uint32_t wait_ms);
const char * zenith_packet_type_to_str(zenith_packet_type_t packet_type);