#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "freertos/event_groups.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "esp_sleep.h"

#include "zenith-now.h"
#include "aht30.h"
#include "zenith-blink.h"

#include "zenith-node.h"

/* zenith specific variables */
static const char *TAG = "zenith-node";
static uint8_t node_peer[ESP_NOW_ETH_ALEN] = { 0 }; // peers mac address
RTC_DATA_ATTR int failed_sends = 0;

esp_err_t remove_node_peer_from_nvs(void)
{
    ESP_LOGI(TAG, "remove_node_peer_from_nvs()");
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("zenith", NVS_READWRITE, &nvs);
    if (err != ESP_OK)
        return err;

    // Store MAC as binary blob
    err = nvs_erase_key(nvs, "core_mac");
    if (err == ESP_OK)
        err = nvs_commit(nvs);
    nvs_close(nvs);
    return err;
}

esp_err_t save_node_peer_to_nvs(void)
{
    ESP_LOGI(TAG, "save_node_peer_to_nvs()");
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("zenith", NVS_READWRITE, &nvs);
    if (err != ESP_OK)
        return err;

    // Store MAC as binary blob
    err = nvs_set_blob(nvs, "core_mac", node_peer, ESP_NOW_ETH_ALEN);
    if (err == ESP_OK)
        err = nvs_commit(nvs);
    nvs_close(nvs);
    return err;
}

// Load MAC from NVS
esp_err_t load_node_peer_from_nvs()
{
    ESP_LOGI(TAG, "load_node_peer_from_nvs()");
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("zenith", NVS_READONLY, &nvs);
    if (err == ESP_OK)
    {
        size_t len = ESP_NOW_ETH_ALEN;
        err = nvs_get_blob(nvs, "core_mac", node_peer, &len);
        nvs_close(nvs);
        if (err == ESP_OK)
        {
            if (len != ESP_NOW_ETH_ALEN)
            {
                // Size differs, NVS corrupted
                err = ESP_ERR_NVS_INVALID_LENGTH;
            }
            else
            {
                err = zenith_now_add_peer(node_peer);
            }
        }
    }
    return err;
}

/// @brief  Enter pairingmode
/// @return ESP_OK: success
/// @todo   Move esp_now code to zenith_now
esp_err_t start_pairing()
{
    ESP_LOGI(TAG, "start_pairing()");
    // Start peering
    uint8_t broadcast[ESP_NOW_ETH_ALEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
    ESP_ERROR_CHECK(zenith_now_add_peer(broadcast));
    {
        zenith_now_packet_t data_packet = { .type = PACKET_PAIRING };
        uint8_t peering_tries = 0;
        // Send peering request
        do
        {
            if (peering_tries++ >= 5)
                esp_deep_sleep(NODE_SLEEP_NO_PEER); // 5 minutes in micro-seconds.
            zenith_blink(BLINK_PAIRING);
            ESP_ERROR_CHECK(zenith_now_send_packet(broadcast, data_packet));
            // wait for peering ack
        }
        while (zenith_now_wait_for_ack(PACKET_PAIRING, 5000) != ESP_OK);
        ESP_ERROR_CHECK(zenith_now_remove_peer(broadcast));
        zenith_blink_stop(BLINK_PAIRING);
    }
    return ESP_OK;
}

esp_err_t configure_node_peer(void)
{
    ESP_LOGI(TAG, "configure_node_peer()");
    // Will load the peer from NVS in final code (stored on pairing), but for now just pair each time we start
    esp_err_t err = load_node_peer_from_nvs(node_peer);
    if (err != ESP_OK)
        err = start_pairing();
    return err;
}

void node_rx_callback(const uint8_t *mac, const zenith_now_packet_t *packet)
{
    ESP_LOGI(TAG, "node_rx_callback()");
    switch (packet->type)
    {
        case PACKET_ACK:
            switch (packet->ack_packet_type)
            {
                case PACKET_PAIRING:
                    // Add peer to list and to variable node_peer
                    ESP_ERROR_CHECK(zenith_now_add_peer(mac));
                    // Store peer in variable
                    memcpy(node_peer, mac, ESP_NOW_ETH_ALEN);
                    // Save to NVS
                    ESP_ERROR_CHECK(save_node_peer_to_nvs());
                    break;
            }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "app_main()");
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    if (cause != ESP_SLEEP_WAKEUP_TIMER)
    {
        // nvs_flash_erase();
        ESP_LOGI(TAG, "debug - Sleeping 30 sec for reflash purposes");
        vTaskDelay(pdMS_TO_TICKS(30000));
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
    // Initialize blink LED
    ESP_ERROR_CHECK(init_zenith_blink(GPIO_NUM_8));
    // Set up zenith-now
    ESP_ERROR_CHECK(configure_zenith_now(node_rx_callback, NULL));
    // Check for paired core, and start pairing if not available
    ESP_ERROR_CHECK(configure_node_peer());
    // Set up the sensor
    ESP_ERROR_CHECK(aht30_init());
    zenith_now_packet_t data_packet = { .type = PACKET_DATA };
    ESP_ERROR_CHECK(aht30_read_sensor(&data_packet.sensor_data.temperature, &data_packet.sensor_data.humidity)); // Example data
    ESP_ERROR_CHECK(zenith_now_send_packet(node_peer, data_packet));
    if (zenith_now_wait_for_ack(PACKET_DATA, 2000) == ESP_OK) {
        failed_sends = 0;
    }
    else 
    {
        failed_sends++;
    }
    if (failed_sends >= 5) {
        ESP_ERROR_CHECK(remove_node_peer_from_nvs());
    }
//    vTaskDelay(pdMS_TO_TICKS(5000)); // debug to see the log
    esp_deep_sleep(3e7);             // 3e8 = 5min * 60sec * 1000ms * 1000us
}