// zenith-core.c
#include <stdio.h>
#include "string.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "nvs_flash.h"

#include "zenith-now.h"
#include "zenith-blink.h"

#include "zenith-core.h"
#include "zenith_ui_core.h"
#include "zenith_registry.h"

#define WS2812_GPIO GPIO_NUM_8

static const char *TAG = "zenith-core";

zenith_registry_handle_t node_registry = NULL;

esp_err_t initialize_nvs(void){
    // Initialize default NVS partition
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // Itsa fucked - erase and retry
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return err;
}


/// @brief Receive callback function for Zenith Core
/// @param mac Mac address of the sender of the packet received
/// @param packet The packet received
static void core_rx_callback(const uint8_t *mac, const zenith_now_packet_t *packet)
{
    switch (packet->type)
    {
        case ZENITH_PACKET_PAIRING:
            // Add peer to registry if new
            if ( zenith_registry_index_of_mac( node_registry, mac ) < 0 )
            {
                zenith_node_t node;
                memcpy(&node.mac, mac, ESP_NOW_ETH_ALEN);
                ESP_ERROR_CHECK( zenith_registry_add( node_registry, node ) );
            }
            // Send pairing ack
            ESP_ERROR_CHECK( zenith_now_send_ack( mac, ZENITH_PACKET_PAIRING ) );
            ESP_ERROR_CHECK( zenith_blink( BLINK_PAIRING_COMPLETE ) );
            break;

        case ZENITH_PACKET_DATA:
            zenith_blink( BLINK_DATA_RECEIVE );
            ESP_LOGI( TAG, "Received data from "MACSTR" : %.2f°  %.2f%%", MAC2STR(mac), packet->sensor_data.temperature, packet->sensor_data.humidity );
            ESP_ERROR_CHECK( zenith_now_send_ack( mac, ZENITH_PACKET_DATA ) );
            break;
    }
}

 void app_main( void )
{
    ESP_LOGI( TAG, "app_main()" );
    // Initialize default NVS partition
    ESP_ERROR_CHECK( initialize_nvs() );
    ESP_ERROR_CHECK( zenith_registry_create( &node_registry ) );
    ESP_ERROR_CHECK( zenith_registry_init( node_registry ) );
    uint8_t count;
    zenith_registry_count (node_registry, &count );
    ESP_LOGI(TAG, "Registry has %d values", count);
    // Initialize display and load UI
    zenith_ui_config_t ui_config = {
        .spi_host = SPI2_HOST,
        .sclk_pin = GPIO_NUM_6,
        .mosi_pin = GPIO_NUM_7,
        .miso_pin = GPIO_NUM_2,

        .lcd_backlight_pin = GPIO_NUM_21,
        .lcd_cs_pin = GPIO_NUM_18,
        .lcd_dc_pin = GPIO_NUM_20,
        .lcd_reset_pin = GPIO_NUM_19,

        .touch_cs_pin = GPIO_NUM_22,
        .touch_irq_pin = GPIO_NUM_NC,

        .node_registry = node_registry,
    };
    zenith_ui_handle_t core_ui_handle = NULL;
    ESP_ERROR_CHECK( zenith_ui_new_core( &ui_config, &core_ui_handle ) );
    ESP_ERROR_CHECK( zenith_ui_test( core_ui_handle ) );
    zenith_ui_core_fade_lcd_brightness(75, 1500);

    // Initialize blinker
    ESP_ERROR_CHECK( init_zenith_blink( WS2812_GPIO ) );

    // Initialize Zenith with default configuration
    ESP_ERROR_CHECK( configure_zenith_now() );

    zenith_now_set_rx_cb( core_rx_callback );

    // Staying alive!
    vTaskDelay(portMAX_DELAY);
} 