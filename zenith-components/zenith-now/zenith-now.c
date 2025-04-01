#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "freertos/event_groups.h"
#include "esp_mac.h"
#include "nvs_flash.h"

#include "zenith-now.h"
#include "zenith-private.h"

QueueHandle_t zenith_now_event_queue = NULL;
EventGroupHandle_t zenith_now_event_group = NULL;
static const char *TAG = "zenith-now";
static zenith_rx_cb_t user_rx_cb = NULL;
static zenith_tx_cb_t user_tx_cb = NULL;

esp_err_t zenith_now_configure_wifi(void)
{
    ESP_LOGI(TAG, "zenith_now_configure_wifi()");
    esp_err_t err = esp_netif_init();
    if (err == ESP_OK) err = esp_event_loop_create_default();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    if (err == ESP_OK) err = esp_wifi_init(&cfg);
    if (err == ESP_OK) err = esp_wifi_set_storage(WIFI_STORAGE_RAM); // Set this to WIFI_STORAGE_FLASH?
    if (err == ESP_OK) err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err == ESP_OK) err = esp_wifi_start();
    if (err == ESP_OK) err = esp_wifi_set_channel(ZENITH_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
    // If I want long range esp-now:
    // ESP_ERROR_CHECK( esp_wifi_set_protocol(ESP_IF_WIFI_STA, WIFI_PROTOCOL_11B|WIFI_PROTOCOL_11G|WIFI_PROTOCOL_11N|WIFI_PROTOCOL_LR) );
    return err;
}

esp_err_t zenith_now_serialize_data(const zenith_now_packet_t *data, uint8_t **serialized_data, uint8_t *size)
{
    ESP_LOGI(TAG, "zenith_now_Serialize_data()");
    // set size
    *size = 1;
    switch (data->type)
    {
    case PACKET_DATA:
        *size += SENSOR_DATA_SIZE * 2;
        break;
    case PACKET_ACK:
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
    case PACKET_DATA:
        memcpy(*serialized_data + 1, &data->sensor_data.temperature, SENSOR_DATA_SIZE);
        memcpy(*serialized_data + 1 + SENSOR_DATA_SIZE, &data->sensor_data.humidity, SENSOR_DATA_SIZE);
        break;
    case PACKET_ACK:
        (*serialized_data)[1] = data->ack_packet_type;
        break;
    }
    return ESP_OK;
}

zenith_now_packet_t zenith_now_deserialize_data(const uint8_t *serialized_data, int len)
{
    ESP_LOGI(TAG, "zenith_now_deserialize_data()");
    assert(len >= 1); // We need at least 1 byte
    // Unpack data from serialized_data into packet
    zenith_now_packet_t data = {0};
    data.type = serialized_data[0];
    switch (data.type)
    {
    case PACKET_DATA:
        if (len >= SENSOR_DATA_SIZE * 2 + 1)
        {
            memcpy(&data.sensor_data.temperature, &serialized_data[1], SENSOR_DATA_SIZE);
            memcpy(&data.sensor_data.humidity, &serialized_data[1 + SENSOR_DATA_SIZE], SENSOR_DATA_SIZE);
        }
        break;
    case PACKET_ACK:
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

const char *zenith_now_packet_type_to_str(zenith_now_packet_type_t packet_type)
{
    if (packet_type < PACKET_PAIRING || packet_type > PACKET_ACK)
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
    ESP_LOGI(TAG, "zenith_now_wait_for_ack()");
    EventBits_t event_bit = 0;
    switch (packet_type)
    {
    case PACKET_PAIRING:
        event_bit = PAIRING_ACK_BIT;
        break;
    case PACKET_DATA:
        event_bit = DATA_ACK_BIT;
        break;
    case PACKET_PING:
        event_bit = PING_ACK_BIT;
        break;
    default:
        ESP_LOGE(TAG, "Illegal packet type for ACK");
        break;
    }
    return (xEventGroupWaitBits(zenith_now_event_group, event_bit, pdTRUE, pdFALSE, pdMS_TO_TICKS(wait_ms)) > 0) ? ESP_OK : ESP_ERR_TIMEOUT;
}

esp_err_t zenith_now_send_ack(const uint8_t *peer_addr, zenith_now_packet_type_t packet_type)
{
    ESP_LOGI(TAG, "zenith_now_send_ack()");
    zenith_now_packet_t ack = {.type = PACKET_ACK, .ack_packet_type = packet_type};
    zenith_now_send_packet(peer_addr, ack);
    return ESP_OK;
}

esp_err_t zenith_now_send_packet(const uint8_t *peer_addr, const zenith_now_packet_t data_packet)
{
    ESP_LOGI(TAG, "zenith_now_Send_packet()");
    uint8_t data_size = 0;
    uint8_t *data = NULL;
    esp_err_t err = zenith_now_serialize_data(&data_packet, &data, &data_size);
    if (err == ESP_OK) {
        err = esp_now_send(peer_addr, data, data_size);
        free(data);
    }
    return err;
}

static void zenith_now_espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    ESP_LOGI(TAG, "zenith_now_espnow_send_cb()");
    zenith_now_event_t event = {.type = SEND_EVENT, .send.status = status};
    memcpy(&event.send.dest_mac, mac_addr, ESP_NOW_ETH_ALEN);
    xQueueSend(zenith_now_event_queue, &event, 0); // Should perhaps wait some tics, but there shouldn't be many packets in the queue and there's room for 10.
}

static void zenith_now_espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len)
{
    ESP_LOGI(TAG, "zenith_now_espnow_recv_cb");
    zenith_now_event_t event = {.type = RECEIVE_EVENT, .receive.data_packet = zenith_now_deserialize_data(data, len)};
    memcpy(&event.receive.source_mac, recv_info->src_addr, ESP_NOW_ETH_ALEN);
    xQueueSend(zenith_now_event_queue, &event, 0); // Should perhaps wait some tics, but there shouldn't be many packets in the queue and there's room for 10.
}

esp_err_t zenith_now_configure_espnow(void)
{
    ESP_LOGI(TAG, "zenith_now_configure_espnow()");
    esp_err_t err = esp_now_init();
    if (err == ESP_OK)
        err = esp_now_register_send_cb(zenith_now_espnow_send_cb);
    if (err == ESP_OK)
        err = esp_now_register_recv_cb(zenith_now_espnow_recv_cb);
    return err;
}

static void zenith_now_event_handler(void *pvParameters)
{
    ESP_LOGI(TAG, "zenith_now_event_handler()");
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
            case PACKET_PING:
                zenith_now_packet_t ack = {.type = PACKET_ACK, .ack_packet_type = PACKET_PING};
                zenith_now_send_packet(event.receive.source_mac, ack);
                break;
            case PACKET_ACK:
                switch (event.receive.data_packet.ack_packet_type)
                {
                case PACKET_DATA:
                    xEventGroupSetBits(zenith_now_event_group, DATA_ACK_BIT);
                    break;
                case PACKET_PING:
                    xEventGroupSetBits(zenith_now_event_group, PING_ACK_BIT);
                    break;
                case PACKET_PAIRING:
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

esp_err_t zenith_now_add_peer(const uint8_t *mac)
{
    ESP_LOGI(TAG, "zenith_now_add_peer()");
    esp_err_t err = ESP_OK; //It's ok to try and add existing peers
    if (!esp_now_is_peer_exist(mac)) {
        esp_now_peer_info_t peer = {.peer_addr = {0}, .channel = ZENITH_WIFI_CHANNEL, .encrypt = false, .ifidx = ESP_IF_WIFI_STA};
        memcpy(peer.peer_addr, mac, ESP_NOW_ETH_ALEN);
        err = esp_now_add_peer(&peer);
    }
    return err;
}

esp_err_t zenith_now_remove_peer(const uint8_t *mac)
{
    ESP_LOGI(TAG, "zenith_now_remove_peer()");
    esp_err_t err = ESP_OK; //It's ok to try and remove non-existant peers
    if (esp_now_is_peer_exist(mac)) err = esp_now_del_peer(mac);
    return err;
}

esp_err_t configure_zenith_now(zenith_rx_cb_t rx_cb, zenith_tx_cb_t tx_cb)
{
    ESP_LOGI(TAG, "configure_zenith_now()");
    user_rx_cb = rx_cb;
    user_tx_cb = tx_cb;
    
    // Create queue
    zenith_now_event_queue = xQueueCreate(10, sizeof(zenith_now_event_t));
    if (!zenith_now_event_queue)
    {
        ESP_LOGE(TAG, "Error creating zenith_now_event_queue");
        return ESP_FAIL;
    }
    // Create event group (flags)
    zenith_now_event_group = xEventGroupCreate();
    if (!zenith_now_event_group)
    {
        ESP_LOGE(TAG, "Error creating zenith_now_event_group");
        return ESP_FAIL;
    }

    // Initialize default NVS partition
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // Itsa fucked - erase and retry
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    // Configure WiFi
    err = zenith_now_configure_wifi();
    if (err == ESP_OK)
        err = zenith_now_configure_espnow();
    if (err == ESP_OK)
        err = xTaskCreate(zenith_now_event_handler, "zn_events", 2048, NULL, tskIDLE_PRIORITY, NULL) == pdPASS ? ESP_OK : ESP_FAIL; // 2k stack - er det nok?
    return err;
}
