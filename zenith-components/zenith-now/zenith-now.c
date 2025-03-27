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

QueueHandle_t zenith_event_queue = NULL;
EventGroupHandle_t zenith_event_group = NULL;
static const char *TAG = "zenith-now";
static const char *zenith_packet_type_str[] = {
    "Pairing",
    "Data",
    "Ping",
    "ACK"};
static zenith_rx_cb_t user_rx_cb = NULL;
static zenith_tx_cb_t user_tx_cb = NULL;

void zenith_configure_wifi(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(ZENITH_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE));
    // If I want long range esp-now:
    // ESP_ERROR_CHECK( esp_wifi_set_protocol(ESP_IF_WIFI_STA, WIFI_PROTOCOL_11B|WIFI_PROTOCOL_11G|WIFI_PROTOCOL_11N|WIFI_PROTOCOL_LR) );
}

uint8_t *zenith_serialize_data(const zenith_packet_t *data, uint8_t *size)
{
    uint8_t *buf = NULL;
    *size = 1;
    switch (data->type)
    {
    case PACKET_DATA:
        *size += SENSOR_DATA_SIZE * 2;
        buf = malloc(*size);
        assert(buf);
        memcpy(&buf[1], &data->sensor_data.temperature, SENSOR_DATA_SIZE);
        memcpy(&buf[1 + SENSOR_DATA_SIZE], &data->sensor_data.humidity, SENSOR_DATA_SIZE);
        break;
    case PACKET_ACK:
        *size += 1;
        buf = malloc(*size);
        assert(buf);
        buf[1] = data->ack_packet_type;
        break;
    case PACKET_PAIRING:
    case PACKET_PING:
        buf = malloc(*size);
        assert(buf);
        // Need no data
        break;
    default: // PACKET_PAIRING, PACKET_PING
        ESP_LOGE(TAG, "Unsupported packet type!");
        return buf;
    }
    buf[0] = data->type;
    return buf;
}

zenith_packet_t zenith_deserialize_data(const uint8_t *buf, int len)
{
    assert(len >= 1); // We need at least 1 byte
    // Unpack data from buf into packet
    zenith_packet_t data = {0};
    data.type = buf[0];
    switch (data.type)
    {
    case PACKET_DATA:
        if (len >= SENSOR_DATA_SIZE * 2 + 1)
        {
            memcpy(&data.sensor_data.temperature, &buf[1], SENSOR_DATA_SIZE);
            memcpy(&data.sensor_data.humidity, &buf[1 + SENSOR_DATA_SIZE], SENSOR_DATA_SIZE);
        }
        break;
    case PACKET_ACK:
        if (len >= 2)
        {
            data.ack_packet_type = buf[1];
        }
        break;
    default:
        break;
    }
    return data;
}

const char *zenith_packet_type_to_str(zenith_packet_type_t packet_type)
{
    if (packet_type < PACKET_PAIRING || packet_type > PACKET_ACK)
    {
        return "Unknown";
    }
    else
    {
        return zenith_packet_type_str[packet_type - 1];
    }
}

bool zenith_wait_for_ack(zenith_packet_type_t packet_type, uint32_t wait_ms)
{
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
    return xEventGroupWaitBits(zenith_event_group, event_bit, pdTRUE, pdFALSE, pdMS_TO_TICKS(wait_ms));
}

void zenith_send_data(const uint8_t *peer_addr, const zenith_packet_t data_packet)
{
    uint8_t data_size = 0;
    uint8_t *data = zenith_serialize_data(&data_packet, &data_size);
    ESP_ERROR_CHECK(esp_now_send(peer_addr, data, data_size));
    free(data);
}

static void zenith_espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    zenith_event_t event = {.type = SEND_EVENT, .send.status = status};
    memcpy(&event.send.dest_mac, mac_addr, ESP_NOW_ETH_ALEN);
    xQueueSend(zenith_event_queue, &event, 0); // Should perhaps wait some tics, but there shouldn't be many packets in the queue and there's room for 10.
}

static void zenith_espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len)
{
    zenith_event_t event = {.type = RECEIVE_EVENT, .receive.data_packet = zenith_deserialize_data(data, len)};
    memcpy(&event.receive.source_mac, recv_info->src_addr, ESP_NOW_ETH_ALEN);
    xQueueSend(zenith_event_queue, &event, 0); // Should perhaps wait some tics, but there shouldn't be many packets in the queue and there's room for 10.
}

void zenith_configure_espnow(void)
{
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(zenith_espnow_send_cb));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(zenith_espnow_recv_cb));
}

static void zenith_event_handler(void *pvParameters)
{
    zenith_event_t event;
    while (xQueueReceive(zenith_event_queue, &event, portMAX_DELAY) == pdTRUE)
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
                zenith_packet_t ack = {.type = PACKET_ACK, .ack_packet_type = PACKET_PING};
                zenith_send_data(event.receive.source_mac, ack);
                break;
            case PACKET_ACK:
                switch (event.receive.data_packet.ack_packet_type)
                {
                case PACKET_DATA:
                    xEventGroupSetBits(zenith_event_group, DATA_ACK_BIT);
                    break;
                case PACKET_PING:
                    xEventGroupSetBits(zenith_event_group, PING_ACK_BIT);
                    break;
                case PACKET_PAIRING:
                    xEventGroupSetBits(zenith_event_group, PAIRING_ACK_BIT);
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

void configure_zenith(zenith_rx_cb_t rx_cb, zenith_tx_cb_t tx_cb)
{
    user_rx_cb = rx_cb;
    user_tx_cb = tx_cb;
    // Create queue
    zenith_event_queue = xQueueCreate(10, sizeof(zenith_event_t));
    assert(zenith_event_queue); // add proper error handling later
    // Create event grou (flags)
    zenith_event_group = xEventGroupCreate();
    assert(zenith_event_group);
    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // Itsa fucked - erase and retry
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    zenith_configure_wifi();
    zenith_configure_espnow();
    xTaskCreate(zenith_event_handler, "zn_events", 2048, NULL, tskIDLE_PRIORITY, NULL); //2k stack - er det nok?
}
