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

QueueHandle_t zenith_now_event_queue = NULL;
EventGroupHandle_t zenith_now_event_group = NULL;
static const char *TAG = "zenith-now";
static zenith_rx_cb_t user_rx_cb = NULL;
static zenith_tx_cb_t user_tx_cb = NULL;

/// @brief Initializes WiFi with parameters needed for Zenith Now
/// @return ESP_OK on success
/// @todo Add error handling and cleanup
esp_err_t zenith_now_configure_wifi( void )
{
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

/// @brief Translate a zenith_now packet into a bytestream, typically this bytestream goes into an esp-now packet.
/// @param data The zentih_now packet
/// @param serialized_data The serialized data
/// @param size The size of the serialized_data
esp_err_t zenith_now_serialize_data(const zenith_now_packet_t *data, uint8_t **serialized_data, uint8_t *size)
{
    esp_err_t ret = ESP_OK;
    #ifdef ZNDEBUG
    ESP_LOGI(TAG, "zenith_now_Serialize_data()");
    #endif
    // set size
    *size = 1;
    switch (data->type)
    {
    case ZENITH_PACKET_DATA:
        *size +=  sizeof( zenith_now_sensor_data_t );
        break;
    case ZENITH_PACKET_ACK:
        *size += 1;
        break;
    }
    // allocate return buffer
    *serialized_data = malloc(*size);
    if (!*serialized_data)
        return ESP_ERR_NO_MEM;
    (*serialized_data)[0] = data->type;
    switch (data->type)
    {
    case ZENITH_PACKET_DATA:
        (*serialized_data)[1] = data->sensor_data.num_points;
        memcpy( *serialized_data +2, data->sensor_data.data_buffer, ZENITH_NOW_MAX_DATA_LEN );
        break;
    case ZENITH_PACKET_ACK:
        (*serialized_data)[1] = data->ack_packet_type;
        break;
    }
    return ret;
}

/// @brief Translate a bytestream into a zenith_now_packet, typically this bytestream comes from esp-now packet.
/// @param serialized_data bytestream representing a zenith_now packet
/// @param len length of the bytestream
/// @return zenith_now packet created from the serialized data
/// @todo maybe make this return esp_err_t like the rest, and have the return value as the last *paramenter. replace assert with real error handling
zenith_now_packet_t zenith_now_deserialize_data( const uint8_t *serialized_data, int len )
{
    ESP_LOGD( TAG, "zenith_now_deserialize_data()" );
    assert( len >= 1 ); // We need at least 1 bytes

    // Unpack data from serialized_data into packet
    zenith_now_packet_t data = {
        .type = serialized_data[ 0 ],
    };

    switch ( data.type ) {
        case ZENITH_PACKET_DATA:
            data.sensor_data.num_points = serialized_data[ 1 ];
            memcpy(&data.sensor_data.data_buffer, serialized_data + 2, len - 2); //This should be heap, not stack. We know how much data there is, so just allocate that big buffer. We will pass on size as number of datapoints
            break;
        case ZENITH_PACKET_ACK:
            if (len >= 2)
            {
                data.ack_packet_type = serialized_data[1];
            }
            break;
        default:
            break;
    }
    return data;
}

/// @brief Converts packet type to string - probably made during early debug phase.
/// @param packet_type packet type to translate
/// @return the string that the packet type translates to
const char *zenith_now_packet_type_to_str(zenith_now_packet_type_t packet_type)
{
    if (packet_type < ZENITH_PACKET_PAIRING || packet_type >= ZENITH_PACKET_MAX)
    {
        return "Unknown";
    }
    else
    {
        return zenith_now_packet_type_str[packet_type - 1];
    }
}

/// @brief Waits for ack with timeout
/// @param packet_type Type of packet to await ack for
/// @param wait_ms Timeout
/// @return ESP_OK if ack was received, ESP_ERR_TIMEOUT if timed out
esp_err_t zenith_now_wait_for_ack(zenith_now_packet_type_t packet_type, uint32_t wait_ms)
{
    #ifdef ZNDEBUG
    ESP_LOGI(TAG, "zenith_now_wait_for_ack()");
    #endif
    EventBits_t event_bit = 0;
    switch (packet_type)
    {
    case ZENITH_PACKET_PAIRING:
        event_bit = PAIRING_ACK_BIT;
        break;
    case ZENITH_PACKET_DATA:
        event_bit = DATA_ACK_BIT;
        break;
    default:
        ESP_LOGE(TAG, "Illegal packet type for ACK");
        return ESP_ERR_INVALID_ARG;
    }
    return (xEventGroupWaitBits(zenith_now_event_group, event_bit, pdTRUE, pdFALSE, pdMS_TO_TICKS(wait_ms)) > 0) ? ESP_OK : ESP_ERR_TIMEOUT;
}

/// @brief Sends an ack to the peer for the packet type supplied
/// @param peer_addr the mac address to send to
/// @param packet_type the packet type to ack
/// @return ESP_OK
esp_err_t zenith_now_send_ack(const uint8_t *peer_addr, zenith_now_packet_type_t packet_type)
{
    #ifdef ZNDEBUG
    ESP_LOGI(TAG, "zenith_now_send_ack()");
    #endif
    zenith_now_packet_t ack = {.type = ZENITH_PACKET_ACK, .ack_packet_type = packet_type};
    zenith_now_send_packet(peer_addr, ack);
    return ESP_OK;
}

/// @brief Sends zenith_now packets over esp-now, and heals the esp-now peer list
/// @param peer_addr mac address we want to send to
/// @param data_packet the zenith_now packet we want to send
/// @return ESP_OK, otherwise passed on error values.
esp_err_t zenith_now_send_packet( const uint8_t *peer_addr, const zenith_now_packet_t data_packet )
{
    esp_err_t ret = ESP_OK;
    ESP_LOGD( TAG, "zenith_now_Send_packet()" );
    // Assure that we can send to this address / heal the list
    if ( !esp_now_is_peer_exist( peer_addr ) )
        ESP_RETURN_ON_ERROR(
            zenith_now_add_peer(peer_addr),
            TAG, "Error adding peer on send"
        );

    uint8_t data_size = 0;
    uint8_t *data = NULL;
    ret = zenith_now_serialize_data( &data_packet, &data, &data_size );

    if (ret == ESP_OK) 
        ret = esp_now_send(peer_addr, data, data_size);
        
    free(data);
    return ret;
}

/// @brief Callback for ESP-NOW send. Posts "sent event" to the event queue.
/// @param mac_addr mac address packet was sent to
/// @param status status of the send
static void zenith_now_espnow_send_cb( const uint8_t *mac_addr, esp_now_send_status_t status )
{
    ESP_LOGD( TAG, "zenith_now_espnow_send_cb()" );
    zenith_now_event_t event = {
        .type = SEND_EVENT, 
        .send.status = status
    };
    memcpy( &event.send.dest_mac, mac_addr, ESP_NOW_ETH_ALEN );
    xQueueSend( zenith_now_event_queue, &event, 0 ); // Should perhaps wait some tics, but there shouldn't be many packets in the queue and there's room for 10.
}

/// @brief Callback for ESP-NOW Receive. Post "receive event" to the event queue.
/// @param recv_info from and to addresses
/// @param data packet data bytestream
/// @param len length of packet data bytestream
static void zenith_now_espnow_recv_cb( const esp_now_recv_info_t *recv_info, const uint8_t *data, int len )
{
    ESP_LOGD(TAG, "zenith_now_espnow_recv_cb");
    zenith_now_event_t event = {
        .type = RECEIVE_EVENT, 
        .receive.data_packet = zenith_now_deserialize_data( data, len ) 
    };
    memcpy( &event.receive.source_mac, recv_info->src_addr, ESP_NOW_ETH_ALEN );
    xQueueSend( zenith_now_event_queue, &event, 0 ); // Should perhaps wait some tics, but there shouldn't be many packets in the queue and there's room for 10.
}

/// @brief Configure ESP-NOW for Zenith Now
/// @details Initialize: esp_now
/// @details Configures: send and receive callbacks
/// @return ESP_OK, or undelying error number.
esp_err_t zenith_now_configure_espnow( void )
{
    ESP_LOGD( TAG, "zenith_now_configure_espnow()" );
    // Initialize esp_now
    esp_err_t err = esp_now_init();
    // Initialize send callback
    if ( err == ESP_OK )
        err = esp_now_register_send_cb( zenith_now_espnow_send_cb );
    // Initialize receive callback
    if ( err == ESP_OK )
        err = esp_now_register_recv_cb( zenith_now_espnow_recv_cb );

    return err;
}

/// @brief The zenith_now event handler task. Handles the events that get posted to the queue by the esp_now_callbacks.
/// @details calls user callbacks and sets event group ack flags.
/// @param pvParameters Currently unused - just pass NULL.
/// @todo add task to name, as this is the task that cointains the event handler.
static void zenith_now_event_handler( void *pvParameters )
{
    ESP_LOGD(TAG, "zenith_now_event_handler()");
    zenith_now_event_t event;
    while (xQueueReceive(zenith_now_event_queue, &event, portMAX_DELAY) == pdTRUE)
    {
        switch (event.type)
        {
        case SEND_EVENT:
            // IDGAF about success - user callback can deal with it
            if (user_tx_cb)
                user_tx_cb(event.send.dest_mac, event.send.status);
            break;
        case RECEIVE_EVENT:
            switch (event.receive.data_packet.type)
            {
            case ZENITH_PACKET_ACK:
                switch (event.receive.data_packet.ack_packet_type)
                {
                case ZENITH_PACKET_DATA:
                    xEventGroupSetBits(zenith_now_event_group, DATA_ACK_BIT);
                    break;
                case ZENITH_PACKET_PAIRING:
                    xEventGroupSetBits(zenith_now_event_group, PAIRING_ACK_BIT);
                    break;
                default:
                    ESP_LOGE(TAG, "ACK unsupported for packet type");
                    break;
                }
                break;
            }
            // Data and Pairing is user_rc_cb stuff, but they can get all and do what they want. I'm done processing it
            if (user_rx_cb)
                user_rx_cb(event.receive.source_mac, &event.receive.data_packet);
            break;
        }
    }
}

/// @brief Add peer to zenith now
/// @param mac address to add
/// @return ESP_OK, or underlying error value
esp_err_t zenith_now_add_peer( const uint8_t *mac )
{
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
esp_err_t zenith_now_remove_peer( const uint8_t *mac )
{
    ESP_LOGD( TAG, "zenith_now_remove_peer()" );

    esp_err_t ret = ESP_OK; //It's ok to try and remove non-existant peers

    if ( esp_now_is_peer_exist( mac ) )
        ret = esp_now_del_peer( mac );

    return ret;
}

/// @brief Sets the zenith_now receive user callback. Called from the event handler.
/// @param rx_cb The receive callback
void zenith_now_set_rx_cb( zenith_rx_cb_t rx_cb ) {
    user_rx_cb = rx_cb;
}

/// @brief Sets the zenith_now send user callback. Called from the event handler.
/// @param tx_cb The send callback
void zenith_now_set_tx_cb( zenith_tx_cb_t tx_cb ) {
    user_tx_cb = tx_cb;
}

/// @brief Initializes zenith_now and starts the data event handler
/// @details Initializes: Event Queue, EventGroup (Flags), nvs
/// @details Configures: WiFi, ESP-NOW
/// @details Starts: Event handler task
/// @return  ESP_OK, passes on underlaying error value
esp_err_t configure_zenith_now( void )
{
    ESP_LOGD( TAG, "configure_zenith_now()" );
    esp_err_t ret = ESP_OK;
    // Create zenith now event queue
    zenith_now_event_queue = xQueueCreate( 10, sizeof( zenith_now_event_t ) );
    ESP_RETURN_ON_FALSE(
        zenith_now_event_queue,
        ESP_FAIL,
        TAG, "Error creating zenith_now_event_queue"
    );

    // Create zenith now event group (flags)
    zenith_now_event_group = xEventGroupCreate();
    ESP_RETURN_ON_FALSE(
        zenith_now_event_group,
        ESP_FAIL,
        TAG, "Error creating zenith_now_event_group"
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
    ESP_RETURN_ON_ERROR(
        zenith_now_configure_espnow(),
        TAG, "Error configuring ESP-NOW"
    );

    ret = xTaskCreate( zenith_now_event_handler, "zn_events", 4096, NULL, tskIDLE_PRIORITY, NULL ) == pdPASS ? ESP_OK : ESP_FAIL; // 2k stack - er det nok? nei, men kankje fire er.
    ESP_RETURN_ON_ERROR(
        ret,
        TAG, "Error creating zenith_now_event_handler task"
    );

    return ret;
}