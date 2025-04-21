// zenith_now.h
#pragma once

//#define ZNDEBUG 1
#define ZENITH_NOW_MAJOR_VERSION 1
#define ZENITH_NOW_MINOR_VERSION 1
#define ZENITH_NOW_VERSION ((ZENITH_NOW_MAJOR_VERSION << 4) | ZENITH_NOW_MINOR_VERSION)

#include "freertos/FreeRTOS.h"
#include "esp_now.h"

#define ZENITH_NOW_PAYLOAD_SIZE( packet_size )  ( ( packet_size ) - sizeof( zenith_now_packet_header_t ) )

/* espnow data */

typedef uint8_t zenith_now_packet_type_t;
enum {
    ZENITH_PACKET_PAIRING = 1,  // Pairing Request
    ZENITH_PACKET_DATA,         // Node data
    ZENITH_PACKET_ACK,          // Acknowledgment for other packet types
    ZENITH_PACKET_MAX           // Yes - for max to exist it has to be above all others. Therefore all usage shall be restricted to under max.
};


typedef struct __attribute__((packed)) zenith_now_payload_ack_s {
    zenith_now_packet_type_t ack_for_type;
} zenith_now_payload_ack_t;

typedef struct __attribute__((packed)) zenith_now_payload_pairing_s {
    uint8_t flags; //  unused - could be stuff like supported zenith now version etc. node firmware version etc.
} zenith_now_payload_pairing_t;

/// @todo: this should probably be defined somewhere else - this is the same type that sensors should use to report.
typedef struct __attribute__((packed)) zenith_node_datapoint_s {
    uint8_t reading_type; // 1 byte
    uint16_t value;       // 2 bytes
} zenith_node_datapoint_t; // zenith_now_data_dataptoint_t?

typedef struct __attribute__((packed)) zenith_now_payload_data_s {
    uint8_t num_points;
    zenith_node_datapoint_t datapoints[];
} zenith_now_payload_data_t;

typedef struct __attribute__((packed)) zenith_now_packet_header_s {
    zenith_now_packet_type_t type; // Packet type (1 byte)
    uint8_t version; // Packet version (1 byte)
} zenith_now_packet_header_t;

typedef struct __attribute__((packed)) zenith_now_packet_s {
    zenith_now_packet_header_t header; // Packet header
    uint8_t payload[]; // Payload (depending on packet type)
} zenith_now_packet_t;

typedef zenith_now_packet_t *zenith_now_packet_handle_t;

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
    zenith_now_packet_t *data_packet;
} zenith_now_receive_event_t;

typedef struct zenith_event {
    zenith_now_event_type_t type;
    union {
        zenith_now_send_event_t send;
        zenith_now_receive_event_t receive;
    };
} zenith_now_event_t;

/* Callbacks */
typedef void ( *zenith_rx_cb_t ) (const uint8_t *mac, const zenith_now_packet_t *packet ); //maybe change this to one callbeck for peering request and one for data 
typedef void ( *zenith_tx_cb_t )( const uint8_t *mac, esp_now_send_status_t status );

/* Macros */

#define ZENITH_WIFI_CHANNEL 1
#define PAIRING_ACK_BIT BIT0
#define DATA_ACK_BIT BIT1
//#define SENSOR_DATA_SIZE (sizeof(zenith_now_sensor_datatype_t))

extern QueueHandle_t zenith_now_event_queue;
extern EventGroupHandle_t zenith_now_event_group;

esp_err_t configure_zenith_now( void );
void zenith_now_set_rx_cb( zenith_rx_cb_t rx_cb );
void zenith_now_set_tx_cb( zenith_tx_cb_t tx_cb );
esp_err_t zenith_now_add_peer( const uint8_t * mac );
esp_err_t zenith_now_remove_peer( const uint8_t *mac );
esp_err_t zenith_now_send_packet( const uint8_t *peer_addr, const zenith_now_packet_t *data_packet );
esp_err_t zenith_now_send_ack( const uint8_t *peer_addr, zenith_now_packet_type_t packet_type );
esp_err_t zenith_now_wait_for_ack( zenith_now_packet_type_t packet_type, uint32_t wait_ms );
const char * zenith_now_packet_type_to_str( zenith_now_packet_type_t packet_type );