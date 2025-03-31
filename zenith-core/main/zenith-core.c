// zenith-core.c
#include <stdio.h>
#include "string.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "nvs_flash.h"

#include "zenith-now.h"
#include "zenith-blink.h"

#include "display.h"

#define WS2812_GPIO GPIO_NUM_8

static const char *TAG = "zenith-core";

/// @brief Receive callback function for Zenith Core
/// @param mac Mac address of the sender of the packet received
/// @param packet The packet received
static void core_rx_callback(const uint8_t *mac, const zenith_now_packet_t *packet)
{
    ESP_LOGI(TAG, "core_rx_callback()");
    switch (packet->type)
    {
        case PACKET_PAIRING:
            zenith_now_add_peer(mac);
            // Send pairing ack
            ESP_ERROR_CHECK(zenith_now_send_ack(mac, PACKET_PAIRING));
            zenith_blink(BLINK_PAIRING_COMPLETE);
            break;

        case PACKET_DATA:
            zenith_blink(BLINK_DATA_RECEIVE);
            // Add any missing peer for fault tolerance
            zenith_now_add_peer(mac);
            display_update_values(packet->sensor_data.temperature, packet->sensor_data.humidity);
            ESP_ERROR_CHECK(zenith_now_send_ack(mac, PACKET_DATA));
            break;
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "app_main()");
    // Initialize default NVS partition
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // Itsa fucked - erase and retry
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_ERROR_CHECK(display_init());
    ESP_ERROR_CHECK(init_zenith_blink(WS2812_GPIO));
    // Initialize Zenith with default configuration
    ESP_ERROR_CHECK(configure_zenith_now(core_rx_callback, NULL));

    // Staying alive!
    vTaskDelay(portMAX_DELAY);
}