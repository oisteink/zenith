#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "freertos/event_groups.h"
#include "esp_mac.h"
#include "nvs_flash.h"

#include "zenith-now.h"
#include "aht30.h"
#include "zenith-blink.h"

/* zenith specific variables */
static const char *TAG = "zenith-node";
static uint8_t node_peer[ESP_NOW_ETH_ALEN] = { 0 }; // peers mac address
static TimerHandle_t node_measuring_timer = NULL;

esp_err_t save_peer_mac(const uint8_t *mac) {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("zenith", NVS_READWRITE, &nvs);
    if (err != ESP_OK) return err;

    // Store MAC as binary blob
    err = nvs_set_blob(nvs, "core_mac", mac, ESP_NOW_ETH_ALEN);
    if (err == ESP_OK) err = nvs_commit(nvs);
    nvs_close(nvs);
    return err;
}

// Load MAC from NVS
esp_err_t load_peer_mac(uint8_t *mac) {
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("zenith", NVS_READONLY, &nvs);
    if (err != ESP_OK) return err;

    size_t len = ESP_NOW_ETH_ALEN;
    err = nvs_get_blob(nvs, "core_mac", mac, &len);
    nvs_close(nvs);
    if (err == ESP_OK && len != ESP_NOW_ETH_ALEN) err = ESP_ERR_NVS_INVALID_LENGTH;
    return err;
}

void configure_node_peer(void)
{
    // Will load the peer from NVS in final code (stored on pairing), but for now just pair each time we start
    if (load_peer_mac(node_peer) != ESP_OK) {
        // Start peering
        esp_now_peer_info_t broadcast = {.channel = ZENITH_WIFI_CHANNEL, .encrypt = false, .ifidx = ESP_IF_WIFI_STA, .peer_addr = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff}};
        ESP_ERROR_CHECK(esp_now_add_peer(&broadcast));
        zenith_packet_t data_packet = {.type = PACKET_PAIRING};
        // Send peering request
        do {
            ESP_LOGI(TAG, "Sending peering request");
            zenith_blink(BLINK_PAIRING);
            zenith_send_data(broadcast.peer_addr, data_packet);
            // wait for peering ack
        } while (!zenith_wait_for_ack(data_packet.type, 5000));
        ESP_ERROR_CHECK(esp_now_del_peer(broadcast.peer_addr));
        zenith_blink_stop(BLINK_PAIRING);
        zenith_blink(BLINK_PAIRING_COMPLETE);
    }
}

void node_measuring_timer_cb(TimerHandle_t xTimer)
{
    ESP_LOGI(TAG, "Read sensor data");
    zenith_packet_t data_packet = { .type = PACKET_DATA };
    aht30_read_sensor(&data_packet.sensor_data.temperature, &data_packet.sensor_data.humidity);    // Example data
    zenith_blink(BLINK_DATA_SEND);
    zenith_send_data(node_peer, data_packet);
    // Check for ack? Maybe increase the send time or something like that, or return to pairing mode.
}

void start_node_measuring_timer(void)
{
    // Create auto-renewing timer with 10 seconds countdown
    node_measuring_timer = xTimerCreate("MEASURE_Timer", pdMS_TO_TICKS(10000), pdTRUE, NULL, node_measuring_timer_cb);
    if (node_measuring_timer == NULL)
    {
        ESP_LOGE(TAG, "Failed to create timer");
    }
    else
    {
        xTimerStart(node_measuring_timer, 0);
    }
    //Run it straight away! nah - that borks everything!
    //node_measuring_timer_cb(node_measuring_timer);
}

void node_rx_callback(const uint8_t *mac, const zenith_packet_t *packet)
{
    switch (packet->type)
    {
    case PACKET_ACK:
        switch (packet->ack_packet_type)
        {
        case PACKET_PAIRING:
            // Add peer to list and to variable node_peer
            ESP_LOGI(TAG, "Got peer: " MACSTR, MAC2STR(mac));
            esp_now_peer_info_t peer = { .peer_addr = {0}, .channel = ZENITH_WIFI_CHANNEL, .encrypt = false, .ifidx = ESP_IF_WIFI_STA };
            memcpy(peer.peer_addr, mac, ESP_NOW_ETH_ALEN);
            ESP_ERROR_CHECK(esp_now_add_peer(&peer));
            memcpy(node_peer, mac, ESP_NOW_ETH_ALEN);
            ESP_ERROR_CHECK(save_peer_mac(mac));
            break;
        }
    }
}

void app_main(void)
{
    // Set up the sensor
    aht30_init();
    init_zenith_blink(8);
    // Set up zenith-now
    configure_zenith(node_rx_callback, NULL);
    // Check for paired core, and start pairing if not available
    configure_node_peer();
    // Start timer that reads sensor and sends values
    start_node_measuring_timer();
    // Don't think I need to sleep here, as the timer runs on low 
}