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

static node_data_t nodes[10] = {0}; 
static uint8_t node_count = 0;

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

esp_err_t nvs_save_paired_nodes(void) {
    ESP_LOGI(TAG, "Saving %i nodes to NVS", node_count);
    nvs_handle_t handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &handle);
    if (err == ESP_OK)
    {
        nvs_node_data_t nvs_nodes = {0};
        nvs_nodes.count = node_count; // set the number of nodes
        for (uint8_t i = 0; i < node_count; i++) {
            memcpy(nvs_nodes.macs[i], nodes[i].mac, ESP_NOW_ETH_ALEN); // copy node mac
        }
        err = nvs_set_blob(handle, "paired_nodes", &nvs_nodes, sizeof(nvs_nodes));
        if (err == ESP_OK) err = nvs_commit(handle);
        nvs_close(handle);
    }
    return err;
}

esp_err_t nvs_load_paired_nodes(void) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &handle); // Create namespace if missing
    if (err == ESP_OK)  
    {
        nvs_node_data_t nvs_nodes;
        size_t required_size = sizeof(nvs_nodes);
        err = nvs_get_blob(handle, "paired_nodes", &nvs_nodes, &required_size);
        if (err == ESP_ERR_NVS_NOT_FOUND) // Blob not found
        {
            memset(&nvs_nodes, 0, sizeof(nvs_nodes)); // Clear / initialize the list
            err = ESP_OK; 
        }
        node_count = nvs_nodes.count;
        ESP_LOGI(TAG, "Loading %i nodes", node_count);
        for (uint8_t i = 0; i < nvs_nodes.count && err == ESP_OK; i++) {
            memcpy(nodes[i].mac, nvs_nodes.macs[i], ESP_NOW_ETH_ALEN);
            err = zenith_now_add_peer(nodes[i].mac);
            if (err == ESP_OK)
                ESP_LOGI(TAG, "Added: "MACSTR, MAC2STR(nodes[i].mac));
        }
        nvs_close(handle);
    }
    return err;
}

bool core_is_peer_exists(const uint8_t *mac_addr)
{
    for (int i = 0; i < node_count; i++) {
        if (memcmp(mac_addr, nodes[i].mac, 6) == 0) {
            return true;
        }
    }
    return false;
}

esp_err_t core_add_new_peer(const uint8_t *mac_addr) {
    // Check if already paired
    if (core_is_peer_exists(mac_addr))
        return ESP_OK;
    // Add new node
    if (node_count >= 10) 
        return ESP_ERR_NO_MEM; // Max nodes reached

    // add peer to esp_now
    esp_err_t err = zenith_now_add_peer(mac_addr);

    if (err == ESP_OK)
    {
        // add peer to nodes
        memcpy(nodes[node_count].mac, mac_addr, ESP_NOW_ETH_ALEN);
        node_count++;

        // save to nvs
        err = nvs_save_paired_nodes();
    }    
    if (err == ESP_OK)
        ESP_LOGI(TAG, "Added new node: "MACSTR, MAC2STR(mac_addr));
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
            ESP_ERROR_CHECK(core_add_new_peer(mac));
            // Send pairing ack
            ESP_ERROR_CHECK(zenith_now_send_ack(mac, ZENITH_PACKET_PAIRING));
            ESP_ERROR_CHECK(zenith_blink(BLINK_PAIRING_COMPLETE));
            break;

        case ZENITH_PACKET_DATA:
            if (!core_is_peer_exists(mac))
            {
                zenith_blink(BLINK_DATA_UNKNOWN_PEER);
                break;
            }
            zenith_blink(BLINK_DATA_RECEIVE);
            ESP_LOGI(TAG, "Received data from "MACSTR" : %.2f°  %.2f%%", MAC2STR(mac), packet->sensor_data.temperature, packet->sensor_data.humidity);
            //display_update_values(packet->sensor_data.temperature, packet->sensor_data.humidity);
            ESP_ERROR_CHECK(zenith_now_send_ack(mac, ZENITH_PACKET_DATA));
            break;
    }
}

 void app_main(void)
{
    ESP_LOGI(TAG, "app_main()");
    // Initialize default NVS partition
    ESP_ERROR_CHECK(initialize_nvs());

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
    };
    zenith_ui_handle_t core_ui_handle = NULL;
    ESP_ERROR_CHECK(zenith_ui_new_core(&ui_config, &core_ui_handle));
    zenith_ui_core_fade_lcd_brightness(75, 1500);

    // Initialize blinker
    ESP_ERROR_CHECK(init_zenith_blink(WS2812_GPIO));

    // Initialize Zenith with default configuration
    ESP_ERROR_CHECK(configure_zenith_now());

    // Load the saved nodes
    ESP_ERROR_CHECK(nvs_load_paired_nodes());

    zenith_now_set_rx_cb(core_rx_callback);

    // Staying alive!
    vTaskDelay(portMAX_DELAY);
} 