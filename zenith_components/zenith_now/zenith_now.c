// zenith_now.c
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "freertos/event_groups.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "esp_check.h"

#include "zenith_now.h"
#include "zenith_private.h"

//QueueHandle_t zenith_now_event_queue = NULL;
//EventGroupHandle_t zenith_now_event_group = NULL;
static const char *TAG = "zenith-now";

static zenith_now_t zenith_now_instance = {
    .config = {0},
    .event_queue = NULL,
    .event_group = NULL,
    .task_handle = NULL,
};
//static zenith_now_receive_callback_t user_rx_cb = NULL;
//static zenith_now_send_callback_t user_tx_cb = NULL;

/// @brief Initializes WiFi with parameters needed for Zenith Now
/// @return ESP_OK on success
/// @todo Add error handling and cleanup
esp_err_t zenith_now_configure_wifi( void ){
    esp_err_t ret = ESP_OK;
    ESP_LOGD( TAG, "zenith_now_configure_wifi()" );

    ESP_RETURN_ON_ERROR(
        esp_netif_init(),
        TAG, "Error initializing netif"
    );

    ESP_RETURN_ON_ERROR(
        esp_event_loop_create_default(),
        TAG, "Error creating event loop"
    );

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(
        esp_wifi_init( &cfg ),
        TAG, "Error initializing WiFi"
    );

    ESP_RETURN_ON_ERROR(
        esp_wifi_set_storage( WIFI_STORAGE_RAM ), // Set this to WIFI_STORAGE_FLASH?
        TAG, "Error setting WiFi storage"
    );

    ESP_RETURN_ON_ERROR(
        esp_wifi_set_mode( WIFI_MODE_STA ), // Set this to WIFI_MODE_APSTA?
        TAG, "Error setting WiFi mode"
    );

    ESP_RETURN_ON_ERROR(
        esp_wifi_start(),
        TAG, "Error starting WiFi"
    );

    ESP_RETURN_ON_ERROR(
        esp_wifi_set_channel( ZENITH_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE), 
        TAG, "Error setting WiFi power save"
    );

    // If I want long range esp-now:
    // ESP_ERROR_CHECK( esp_wifi_set_protocol(ESP_IF_WIFI_STA, WIFI_PROTOCOL_11B|WIFI_PROTOCOL_11G|WIFI_PROTOCOL_11N|WIFI_PROTOCOL_LR) );
    return ret;
}

/// @brief Converts packet type to string - probably made during early debug phase.
/// @param packet_type packet type to translate
/// @return the string that the packet type translates to
const char *zenith_now_packet_type_to_str(zenith_now_packet_type_t packet_type) {
    if (packet_type < ZENITH_PACKET_PAIRING || packet_type >= ZENITH_PACKET_MAX)
    {
        return "Unknown";
    }
    else
    {
        return zenith_now_packet_type_str[packet_type - 1];
    }
}

int get_payload_size( const zenith_now_packet_t *data_packet ) {
    int payload_size = 0;
    switch ( data_packet->header.type ) {
        case ZENITH_PACKET_PAIRING:
            ESP_LOGD( TAG, "Pairing packet" );
            payload_size = sizeof( zenith_now_payload_pairing_t );
            break;
        case ZENITH_PACKET_DATA:
            ESP_LOGD( TAG, "Data packet" );
            zenith_now_payload_data_t *data_payload = ( zenith_now_payload_data_t * ) data_packet->payload;
            payload_size = sizeof( zenith_now_payload_data_t ) + sizeof(zenith_node_datapoint_t) * data_payload->num_points;
            break;
        case ZENITH_PACKET_ACK:
            ESP_LOGD( TAG, "Ack packet" );
            payload_size = sizeof( zenith_now_payload_ack_t );
            break;
        default:
            ESP_LOGE( TAG, "Unimplemented packet type" );
            return ESP_ERR_INVALID_ARG;
    }
    return payload_size;
}

/// @brief Add peer to zenith now
/// @param mac address to add
/// @return ESP_OK, or underlying error value
esp_err_t zenith_now_add_peer( const uint8_t *mac ) {
    ESP_LOGD( TAG, "zenith_now_add_peer()" );

    esp_err_t ret = ESP_OK; //It's ok to try and add existing peers

    if ( !esp_now_is_peer_exist( mac ) ) {
        esp_now_peer_info_t peer = {
            .peer_addr = { 0 }, 
            .channel = ZENITH_WIFI_CHANNEL, 
            .encrypt = false, 
            .ifidx = ESP_IF_WIFI_STA
        };
        memcpy( peer.peer_addr, mac, ESP_NOW_ETH_ALEN );
        ret = esp_now_add_peer( &peer );
    }

    return ret;
}

/// @brief Removes a peer from zenith_now
/// @param mac address to remove
/// @return ESP_OK, or underlying error value
esp_err_t zenith_now_remove_peer( const uint8_t *mac ) {
    ESP_LOGD( TAG, "zenith_now_remove_peer()" );

    esp_err_t ret = ESP_OK; //It's ok to try and remove non-existant peers

    if ( esp_now_is_peer_exist( mac ) )
        ret = esp_now_del_peer( mac );

    return ret;
}

bool zenith_now_is_peer_known( const uint8_t *peer_id ) {
    ESP_LOGD( TAG, "zenith_now_is_peer_known()" );
    return esp_now_is_peer_exist( peer_id );
}

/// @brief Waits for ack with timeout
/// @param packet_type Type of packet to await ack for
/// @param wait_ms Timeout
/// @return ESP_OK if ack was received, ESP_ERR_TIMEOUT if timed out
esp_err_t zenith_now_wait_for_ack( zenith_now_packet_type_t packet_type, uint32_t wait_ms ) {
    ESP_LOGD( TAG, "zenith_now_wait_for_ack()" );

    EventBits_t event_bit = 0;

    switch ( packet_type ) {
        case ZENITH_PACKET_PAIRING:
            event_bit = PAIRING_ACK_BIT;
            break;

        case ZENITH_PACKET_DATA:
            event_bit = DATA_ACK_BIT;
            break;

        default:
            ESP_LOGE( TAG, "Illegal packet type for ACK" );
            return ESP_ERR_INVALID_ARG;
    }
    return 
        ( xEventGroupWaitBits( zenith_now_instance.event_group, event_bit, pdTRUE, pdFALSE, pdMS_TO_TICKS( wait_ms ) ) > 0 ) 
        ? ESP_OK 
        : ESP_ERR_TIMEOUT;
}

/// @brief Sends a zenith_now packet over esp-now
/// @param peer_mac Peer to send to
/// @param data_payload the payload for the data packet
/// @return 
esp_err_t zenith_now_send_data( const uint8_t *peer_mac, const zenith_now_payload_data_t *data_payload ) {
    esp_err_t ret = ESP_OK;
    ESP_LOGI(TAG, "zenith_now_send_data()");

    size_t payload_size = sizeof( zenith_now_payload_data_t ) + sizeof( zenith_node_datapoint_t ) * data_payload->num_points;
    size_t packet_size = sizeof( zenith_now_packet_t ) + payload_size;
    ESP_LOGI( TAG, "Createing packet\tsize: %d type: %d", packet_size, ZENITH_PACKET_DATA );

    zenith_now_packet_t *data_packet = calloc( 1, packet_size);
    ESP_RETURN_ON_FALSE(
        data_packet,
        ESP_ERR_NO_MEM,
        TAG, "Error allocating memory for data packet"
    );

    data_packet->header.type = ZENITH_PACKET_DATA;
    data_packet->header.payload_size = payload_size;
    data_packet->header.version = ZENITH_NOW_VERSION;

    memcpy( data_packet->payload, data_payload, payload_size );

    ESP_LOGI(TAG, "sending this packet:");
    ESP_LOG_BUFFER_HEX_LEVEL( TAG, ( uint8_t * ) data_packet, packet_size, ESP_LOG_INFO );
    ret = zenith_now_send_packet( peer_mac, data_packet );

    free( data_packet );
    return ret;
}

/// @brief Sends an ack to the peer for the packet type supplied
/// @param peer_addr the mac address to send to
/// @param packet_type the packet type to ack
/// @return ESP_OK
esp_err_t zenith_now_send_ack( const uint8_t *peer_mac, zenith_now_packet_type_t ack_type ) {
    esp_err_t ret = ESP_OK;
    ESP_LOGI(TAG, "zenith_now_send_ack()");

    size_t payload_size = sizeof( zenith_now_payload_ack_t );
    size_t packet_size = sizeof( zenith_now_packet_t ) + payload_size;
    ESP_LOGI( TAG, "Createing packet\tsize: %d type: %d", packet_size, ack_type );

    zenith_now_packet_t *ack = calloc( 1, packet_size);
    ESP_RETURN_ON_FALSE(
        ack,
        ESP_ERR_NO_MEM,
        TAG, "Error allocating memory for ack packet"
    );

    ack->header.type = ZENITH_PACKET_ACK;
    ack->header.payload_size = payload_size;
    ack->header.version = ZENITH_NOW_VERSION;

    (( zenith_now_payload_ack_t * ) ack->payload)->ack_for_type = ack_type;

    ESP_LOGI(TAG, "sending this packet to ack:");
    ESP_LOG_BUFFER_HEX_LEVEL( TAG, ( uint8_t * ) ack, packet_size, ESP_LOG_INFO );
    ret = zenith_now_send_packet( peer_mac, ack);

    free(ack);
    return ret;
}

/// @brief Currently you can only pair with Zenith Core. This is typically used by the Zenith Node when it needs to pair.
esp_err_t zenith_now_send_pairing( const uint8_t *peer_mac ) {
    esp_err_t ret = ESP_OK;
    ESP_LOGI(TAG, "zenith_now_send_pairing()");

    size_t payload_size = sizeof( zenith_now_payload_pairing_t );
    size_t packet_size = sizeof( zenith_now_packet_t ) + payload_size;
    ESP_LOGI( TAG, "Createing packet\tsize: %d type: %d", packet_size, ZENITH_PACKET_PAIRING );

    zenith_now_packet_t *pairing = calloc( 1, packet_size);
    ESP_RETURN_ON_FALSE(
        pairing,
        ESP_ERR_NO_MEM,
        TAG, "Error allocating memory for pairing packet"
    );

    pairing->header.type = ZENITH_PACKET_PAIRING;
    pairing->header.payload_size = payload_size;
    pairing->header.version = ZENITH_NOW_VERSION;

    (( zenith_now_payload_pairing_t * ) pairing->payload)->flags = 0;

    ESP_LOGI(TAG, "sending this packet to pair: "MACSTR, MAC2STR(peer_mac));
    ESP_LOG_BUFFER_HEX_LEVEL( TAG, ( uint8_t * ) pairing, packet_size, ESP_LOG_INFO );
    ret = zenith_now_send_packet( peer_mac, pairing);

    free(pairing);
    return ret;
}

/// @brief Sends zenith_now packets over esp-now, and heals the esp-now peer list
/// @param peer_addr mac address we want to send to
/// @param data_packet the zenith_now packet we want to send
/// @return ESP_OK, otherwise passed on error values.
esp_err_t zenith_now_send_packet( const uint8_t *peer_mac, const zenith_now_packet_t *packet ) {

    ESP_LOGD(TAG, "zenith_now_send_packet()");
    esp_err_t ret = ESP_OK;

    ESP_RETURN_ON_FALSE(
        peer_mac && packet,
        ESP_ERR_INVALID_ARG,
        TAG, "NULL pointer passed to zenith_now_send_packet"
    );

    if ( !esp_now_is_peer_exist( peer_mac ) )
        ESP_RETURN_ON_ERROR(
            zenith_now_add_peer( peer_mac ),
            TAG, "Error adding esp_now peer during send"
        );

    size_t payload_size = get_payload_size( packet );
    size_t packet_size = sizeof( zenith_now_packet_t ) + payload_size;

    ret = esp_now_send( peer_mac, ( const uint8_t * ) packet, packet_size );

    return ret;
}

/// @brief Callback for ESP-NOW send. Posts "sent event" to the event queue.
/// @param mac_addr mac address packet was sent to
/// @param status status of the send
static void zenith_now_espnow_send_cb( const uint8_t *mac_addr, esp_now_send_status_t status ) {
    ESP_LOGD( TAG, "zenith_now_espnow_send_cb()" );
    zenith_now_event_t event = {
        .type = SEND_EVENT, 
        .send.status = status
    };
    memcpy( &event.send.dest_mac, mac_addr, ESP_NOW_ETH_ALEN );
    xQueueSend( zenith_now_instance.event_queue, &event, 0 ); // Should perhaps wait some tics, but there shouldn't be many packets in the queue and there's room for 10.
}

/// @brief Callback for ESP-NOW Receive. Post "receive event" to the event queue.
/// @param recv_info from and to addresses
/// @param data packet data bytestream
/// @param len length of packet data bytestream
static void zenith_now_espnow_recv_cb( const esp_now_recv_info_t *recv_info, const uint8_t *data, int len ) {
    ESP_LOGD(TAG, "zenith_now_espnow_recv_cb");

    zenith_now_packet_t *packet = malloc( len ); // Free'd in the event handler
    if ( !packet ) {
        ESP_LOGE( TAG, "Failed to allocate memory for received packet" );
        return;
    }
    // Copy the the packet   
    memcpy( packet, data, len );

    ESP_LOGI( TAG, "Received packet type: %d from: "MACSTR, packet->header.type, MAC2STR( recv_info->src_addr ) );
    ESP_LOG_BUFFER_HEX_LEVEL( TAG, ( uint8_t * ) data, len, ESP_LOG_INFO );

    zenith_now_event_t event = {
        .type = RECEIVE_EVENT, 
        .receive.data_packet = packet,
    };
    memcpy( &event.receive.source_mac, recv_info->src_addr, ESP_NOW_ETH_ALEN );
    xQueueSend( zenith_now_instance.event_queue, &event, 0 ); // Should perhaps wait some tics, but there shouldn't be many packets in the queue and there's room for 10.
}

/// @brief The zenith_now event handler task. Handles the events that get posted to the queue by the esp_now_callbacks.
/// @details calls user callbacks and sets event group ack flags.
/// @param pvParameters Currently unused - just pass NULL.
/// @todo add task to name, as this is the task that cointains the event handler.
static void zenith_now_event_handler( void *pvParameters ) {
    ESP_LOGD( TAG, "zenith_now_event_handler()" );
    zenith_now_event_t event;
    while ( xQueueReceive( zenith_now_instance.event_queue, &event, portMAX_DELAY ) == pdTRUE ) {
        switch ( event.type ) {
            case SEND_EVENT:
                // IDGAF about success - user callback can deal with it
                if ( zenith_now_instance.config.tx_cb )
                zenith_now_instance.config.tx_cb ( event.send.dest_mac, event.send.status );
                break;
            case RECEIVE_EVENT:
                if ( event.receive.data_packet->header.type == ZENITH_PACKET_ACK) {
                    zenith_now_payload_ack_t *ack_payload = ( zenith_now_payload_ack_t * ) event.receive.data_packet->payload;
                    switch ( ack_payload->ack_for_type ) {
                
                        case ZENITH_PACKET_DATA:
                            xEventGroupSetBits(zenith_now_instance.event_group, DATA_ACK_BIT);
                            break;

                        case ZENITH_PACKET_PAIRING:
                            xEventGroupSetBits(zenith_now_instance.event_group, PAIRING_ACK_BIT);
                            break;

                        default:
                            ESP_LOGE(TAG, "ACK unsupported for packet type %d", ack_payload->ack_for_type);
                            break;

                    }
                }

                // Hand the packet over to the user callback
                if ( zenith_now_instance.config.rx_cb )
                    zenith_now_instance.config.rx_cb( event.receive.source_mac, event.receive.data_packet );

                // Free the data packet created in the receive callback
                free( event.receive.data_packet );
                break;
            default:
                ESP_LOGE(TAG, "Unknown event type");
                break;
        }
        ESP_LOGD(TAG, "Event handled");
    }
}

/// @brief Initialize zenith_now
/// @details This function initializes the zenith_now library. It initializes the NVS, configures WiFi, and creates the event handler task.
esp_err_t zenith_now_init( const zenith_now_config_t *config ) {
    ESP_LOGD( TAG, "zenith_now_init()" );
    esp_err_t ret;

    memcpy( &zenith_now_instance.config, config, sizeof( zenith_now_config_t ) );

    zenith_now_instance.event_queue = xQueueCreate( 10, sizeof( zenith_now_event_t ) );
    ESP_RETURN_ON_FALSE(
        zenith_now_instance.event_queue,
        ESP_ERR_NO_MEM,
        TAG, "Error creating event queue"
    );

    zenith_now_instance.event_group = xEventGroupCreate();
    ESP_RETURN_ON_FALSE(
        zenith_now_instance.event_group,
        ESP_ERR_NO_MEM,
        TAG, "Error creating event group"
    );

    // Initialize default NVS partition
    ret = nvs_flash_init();
    if ( ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND )
    {
        // Itsa fucked - erase and retry
        ESP_ERROR_CHECK( nvs_flash_erase() );
        ret = nvs_flash_init();
    }
    ESP_RETURN_ON_ERROR(
        ret,
        TAG, "Error initializing NVS"
    );

    // Configure WiFi
    ESP_RETURN_ON_ERROR(
        zenith_now_configure_wifi(),
        TAG, "Error configuring WiFi"
    );

    // Initialize zenith now
    ret = esp_now_init();
    ESP_RETURN_ON_ERROR(
        ret,
        TAG, "Error configuring zenith_now"
    );

    // add the callbacks
    ESP_RETURN_ON_ERROR(
        esp_now_register_recv_cb (zenith_now_espnow_recv_cb ),
        TAG, "Error registering zenith_now receive callback"
    );  
    ESP_RETURN_ON_ERROR(
        esp_now_register_send_cb( zenith_now_espnow_send_cb ),
        TAG, "Error registering zenith_now send callback"
    );  

    // Create event handler - How do I keep up communictaion? I guess the caller should do it, but perhaps the zenith_now could handle data in and out itself by modifying the registry on data receipt?
    ret = xTaskCreate( zenith_now_event_handler, "zn_events", 4096, NULL, tskIDLE_PRIORITY, &zenith_now_instance.task_handle ) == pdPASS ? ESP_OK : ESP_FAIL; // 2k stack - er det nok? nei, men kankje fire er.
    ESP_RETURN_ON_ERROR(
        ret,
        TAG, "Error creating zenith_now_event_handler task"
    );
    ESP_LOGD(TAG, "Zenith now initialized, event handler task is running");
    return ret;
}