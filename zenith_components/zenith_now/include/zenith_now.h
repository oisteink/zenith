// zenith_now.h
#pragma once

//#define ZNDEBUG 1
#define ZENITH_NOW_MAJOR_VERSION 1
#define ZENITH_NOW_MINOR_VERSION 2
#define ZENITH_NOW_VERSION ((ZENITH_NOW_MAJOR_VERSION << 4) | ZENITH_NOW_MINOR_VERSION)

#define ZENITH_WIFI_CHANNEL 1
#define PAIRING_ACK_BIT BIT0
#define DATA_ACK_BIT BIT1


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

/// @brief Zenith Now ack packet payload.
typedef struct __attribute__((packed)) zenith_now_payload_ack_s {
    zenith_now_packet_type_t ack_for_type;
} zenith_now_payload_ack_t;

/// @brief Zenith Now pairing packet payload.
typedef struct __attribute__((packed)) zenith_now_payload_pairing_s {
    uint8_t flags; //  unused - could be stuff like supported zenith now version etc. node firmware version etc.
} zenith_now_payload_pairing_t;

/// @todo: this should probably be defined somewhere else - this is the same type that sensors should use to report.
/// @brief Zenith Now node datapoint.
/// @details This structure is used to represent a single data point from a node. It contains the value of the reading and the type of the reading.
typedef struct __attribute__((packed)) zenith_node_datapoint_s {
    /// @brief The type of the datapoint.
    /// @details This is a 1-byte value that indicates the type of datapoint.
    uint8_t reading_type;
    /// @brief The value of the datapoint.
    /// @details This is a 2-byte value that represents the actual datapoint.
    uint16_t value;      
} zenith_node_datapoint_t; // zenith_now_data_dataptoint_t?

/// @brief Zenith Now data packet payload.
/// @details This structure is used to represent a data packet that contains multiple datapoints. The number of datapoints is specified in the num_points field.
typedef struct __attribute__((packed)) zenith_now_payload_data_s {
    uint8_t num_points;
    zenith_node_datapoint_t datapoints[];
} zenith_now_payload_data_t;

/// @brief Zenith Now packet header.
/// @details This structure is used to represent the header of a Zenith Now packet. It contains the type and version of the packet.
typedef struct __attribute__((packed)) zenith_now_packet_header_s {
    zenith_now_packet_type_t type; // Packet type (1 byte)
    uint8_t version; // Packet version (1 byte)
    uint16_t payload_size; // Payload size (2 bytes)
} zenith_now_packet_header_t;

/// @brief Zenith Now packet.
/// @details This structure is used to represent a Zenith Now packet. It contains the header and the payload. The payload size and format is determined by the type of packet.
typedef struct __attribute__((packed)) zenith_now_packet_s {
    zenith_now_packet_header_t header; // Packet header
    uint8_t payload[]; // Payload (depending on packet type)
} zenith_now_packet_t;

typedef zenith_now_packet_t *zenith_now_packet_handle_t;

/* Structures for xQueue handling */

/// @brief Zenith Now event types.
/// @details This enum is used to represent the different types of events that can occur in the Zenith Now eventqueue. 
typedef enum zenith_now_event_type_e {
    SEND_EVENT = 1,
    RECEIVE_EVENT = 2
} zenith_now_event_type_t;

/// @brief Zenith Now send event.
/// @details This structure is used to represent a send event in the Zenith Now eventqueue. It contains the destination MAC address and the status of the send operation. I'm not totally sure about how to use these yet, but maybe this is where I should start to check for acks.
typedef struct zenith_now_send_event_s {
    uint8_t dest_mac[ESP_NOW_ETH_ALEN];
    esp_now_send_status_t status;
} zenith_now_send_event_t;

/// @brief Zenith Now receive event.
/// @details This structure is used to represent a receive event in the Zenith Now eventqueue. It contains the source MAC address and a pointer to the data packet that was received.
typedef struct zenith_now_receive_event_s {
    uint8_t source_mac[ESP_NOW_ETH_ALEN];
    zenith_now_packet_t *data_packet;
} zenith_now_receive_event_t;

/// @brief Zenith Now event.
/// @details This structure is used to represent an event in the Zenith Now eventqueue. It contains the type of event and a union that can hold either a send event or a receive event. Ive grown kinda tired of unions already and might just make this a dull pointer.
typedef struct zenith_event_s {
    zenith_now_event_type_t type;
    union {
        zenith_now_send_event_t send;
        zenith_now_receive_event_t receive;
    };
} zenith_now_event_t;

/* Callbacks */
typedef void ( *zenith_now_receive_callback_t ) ( const uint8_t *mac_addr, const zenith_now_packet_t *packet );
typedef void ( *zenith_now_send_callback_t ) ( const uint8_t *mac_addr, esp_now_send_status_t status );


typedef struct zenith_now_config_s {
    zenith_now_receive_callback_t rx_cb; // Receive callback
    zenith_now_send_callback_t tx_cb;   // Send callback
    // uint8_t version; // Not sure if this should be option just yet. should always be the defined version.
    // Add other options here like max queue length, debug level, etc.
} zenith_now_config_t;


/// @brief Zenith Now configuration.
typedef struct zenith_now_s {
    /// @brief Store the configuration.
    zenith_now_config_t config;
    /// @brief Store the event queue.
    QueueHandle_t event_queue;
    /// @brief Store the event group used for acks currently. 
    EventGroupHandle_t event_group;
    /// @brief Store the event handler task.
    TaskHandle_t task_handle;
} zenith_now_t;


typedef struct zenith_now_s *zenith_now_handle_t;

esp_err_t zenith_now_init( const zenith_now_config_t *config );  // Initialize everything!
esp_err_t zenith_now_teardown( void) ;                            // Frees shit and stops wifi

// Sending packets
esp_err_t zenith_now_send_ack( const uint8_t *peer_mac, zenith_now_packet_type_t ack_type );
esp_err_t zenith_now_send_pairing( const uint8_t *peer_mac );
esp_err_t zenith_now_send_data( const uint8_t *peer_mac, const zenith_now_payload_data_t *data_payload );

// Low-level generic packet sending (if needed)
esp_err_t zenith_now_send_packet( const uint8_t *peer_mac, const zenith_now_packet_t *packet );

// Utility
esp_err_t zenith_now_add_peer( const uint8_t *peer_mac );
esp_err_t zenith_now_remove_peer( const uint8_t *peer_mac );
bool zenith_now_is_peer_known( const uint8_t *peer_id );

// ACK waiting helper
esp_err_t zenith_now_wait_for_ack( zenith_now_packet_type_t packet_type, uint32_t wait_ms );




