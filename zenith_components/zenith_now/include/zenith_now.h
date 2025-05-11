/**
 * @file zenith_now.h
 * @brief Zenith Network Protocol (ZENITH-NOW) implementation
 * @details This file contains the implementation of the ZENITH-NOW protocol,
 *          which is used for communication between Zenith Core and Node devices.
 *
 * @copyright [Your Copyright Information]
 */
#pragma once

/**
 * @brief Enable debug mode
 * @note Uncomment to enable debug logging
 */
//#define ZNDEBUG 1

/**
 * @brief Major version number of the ZENITH-NOW protocol
 */
#define ZENITH_NOW_MAJOR_VERSION 1

/**
 * @brief Minor version number of the ZENITH-NOW protocol
 */
#define ZENITH_NOW_MINOR_VERSION 2

/**
 * @brief Combined version number (major << 4 | minor)
 */
#define ZENITH_NOW_VERSION ((ZENITH_NOW_MAJOR_VERSION << 4) | ZENITH_NOW_MINOR_VERSION)

#define ZENITH_WIFI_CHANNEL 1
#define PAIRING_ACK_BIT BIT0
#define DATA_ACK_BIT BIT1


#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_now.h"
#include "esp_err.h"
#include "esp_mac.h"

#define ZENITH_NOW_PAYLOAD_SIZE( packet_size )  ( ( packet_size ) - sizeof( zenith_now_packet_header_t ) )

/* espnow data */

/// @brief Type definition for packet types
/// @note This type is used to identify different types of ZENITH-NOW packets
typedef uint8_t zenith_now_packet_type_t;

/**
 * @brief Packet type definitions
 * @details These constants define the different types of packets that can be sent
 *          through the ZENITH-NOW protocol.
 */
enum {
    /** @brief Packet type for pairing requests */
    ZENITH_PACKET_PAIRING = 1,
    /** @brief Packet type for node data */
    ZENITH_PACKET_DATA,
    /** @brief Packet type for acknowledgments */
    ZENITH_PACKET_ACK,
    /** @brief Maximum packet type value */
    ZENITH_PACKET_MAX
};

/// @brief Zenith Now data packet payload is just a zenith_datapoints_t in disguise.
typedef struct zenith_datapoints_s zenith_now_payload_data_t;

/// @brief Zenith Now ack packet payload.
typedef struct __attribute__((packed)) zenith_now_payload_ack_s {
    zenith_now_packet_type_t ack_for_type;
} zenith_now_payload_ack_t;

/// @brief Zenith Now pairing packet payload.
typedef struct __attribute__((packed)) zenith_now_payload_pairing_s {
    uint8_t flags; //  unused - could be stuff like supported zenith now version etc. node firmware version etc.
} zenith_now_payload_pairing_t;

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


/**
 * @brief Initialize the ZENITH-NOW protocol
 * @param config Configuration structure for the protocol
 * @return ESP_OK on success, ESP_ERR_* on failure
 * @note This function must be called before using any other ZENITH-NOW functions
 */
esp_err_t zenith_now_init( const zenith_now_config_t *config );

/**
 * @brief Cleanup and teardown the ZENITH-NOW protocol
 * @return ESP_OK on success, ESP_ERR_* on failure
 * @note This function should be called when the protocol is no longer needed
 */
esp_err_t zenith_now_teardown( void );

// Sending packets
esp_err_t zenith_now_new_packet( zenith_now_packet_type_t packet_type , uint8_t num_datapoints, zenith_now_packet_handle_t *out_packet );
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