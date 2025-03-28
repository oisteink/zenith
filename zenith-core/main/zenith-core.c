// zenith-core.c
#include <stdio.h>
#include "string.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "led_indicator.h"

#include "zenith-now.h"
#include "zenith-blink.h"

#include "display.h"

#define WS2812_GPIO GPIO_NUM_8

static const char *TAG = "zenith-core";


static void core_rx_callback(const uint8_t *mac, const zenith_packet_t *packet)
{
    switch(packet->type) {
        case PACKET_PAIRING:
            ESP_LOGI(TAG, "Got new peer "MACSTR, MAC2STR(mac));
            // Add peer
            esp_now_peer_info_t peer = { .channel = ZENITH_WIFI_CHANNEL, .encrypt = false, .ifidx = ESP_IF_WIFI_STA };
            memcpy(peer.peer_addr, mac, ESP_NOW_ETH_ALEN);
            if (!esp_now_is_peer_exist(mac))  //handle existing peer
            {
                esp_err_t ret = esp_now_add_peer(&peer);
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to add peer: %s", esp_err_to_name(ret));
                    return;
                }
            }
            // Send pairing ack
            zenith_packet_t ack = {.type = PACKET_ACK, .ack_packet_type = PACKET_PAIRING};
            zenith_send_data(mac, ack);
            zenith_blink(BLINK_PAIRING_COMPLETE);
            break;

        case PACKET_DATA:
            zenith_blink(BLINK_DATA_RECEIVE);
            ESP_LOGI(TAG, "Sensor data received from "MACSTR" - Temp: %.1fC, Hum: %.1f%%", MAC2STR(mac), packet->sensor_data.temperature, packet->sensor_data.humidity);
            display_update_values(packet->sensor_data.temperature, packet->sensor_data.humidity);
            break;

        default:
            ESP_LOGE(TAG, "Received unhandled packet type: %s", zenith_packet_type_to_str(packet->type));
            break;
    }
}



void app_main(void)
{
    display_init();
    init_led(WS2812_GPIO);
    // Initialize Zenith with default configuration
    configure_zenith(core_rx_callback, NULL);
 
    // Staying alive!
    vTaskDelay(portMAX_DELAY);
}