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
RTC_DATA_ATTR static uint8_t node_peer[ESP_NOW_ETH_ALEN] = { 0 }; // peers mac address
RTC_DATA_ATTR uint8_t failed_sends = 0;


bool saved_peer(void) {
    uint8_t empty_mac[ESP_NOW_ETH_ALEN] = {0};
    return (memcmp(node_peer, empty_mac, ESP_NOW_ETH_ALEN) != 0);
}

/// @brief  Enter pairingmode
/// @return Allways returns ESP_OK
void pair_with_core(void)
{
    uint8_t broadcast[ESP_NOW_ETH_ALEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
    ESP_ERROR_CHECK(zenith_now_add_peer(broadcast)); // Add broadcast peer
    zenith_now_packet_t data_packet = { .type = PACKET_PAIRING }; // Create pairing packet
    uint8_t peering_tries = 0; // initialize counter for pairing retries
    do
    {
        if (peering_tries++ >= 5)
            esp_deep_sleep(NODE_SLEEP_NO_PEER); // If we miss 5 pairing requests, enter deeps sleep
        ESP_ERROR_CHECK(zenith_blink(BLINK_PAIRING)); // Blink to show we are trying to pair
        ESP_ERROR_CHECK(zenith_now_send_packet(broadcast, data_packet)); // Send the pairing request
    } 
    while (zenith_now_wait_for_ack(PACKET_PAIRING, 5000) != ESP_OK); // Wait 5 seconds for ack, and retry if we timed out
    ESP_ERROR_CHECK(zenith_now_remove_peer(broadcast)); // Remove broadcast peer
    while (!saved_peer())  // Wait for node_peer to be stored. 
        vTaskDelay(pdMS_TO_TICKS(50));
}

/// @brief Reads the sensor and sends the data to the node_peer
/// @return Allways returns ESP_OK
void send_data(void) {
    // Set up the sensor
    ESP_ERROR_CHECK(aht30_init()); // Initialize sensor
    zenith_now_packet_t data_packet = { .type = PACKET_DATA }; // Initialize data packet
    ESP_ERROR_CHECK(aht30_read_sensor(&data_packet.sensor_data.temperature, &data_packet.sensor_data.humidity)); // Read sensor data into data packet
    ESP_ERROR_CHECK(zenith_now_add_peer(node_peer));
    ESP_ERROR_CHECK(zenith_now_send_packet(node_peer, data_packet)); // Send data packet
    failed_sends = (zenith_now_wait_for_ack(PACKET_DATA, 2000) == ESP_OK) ? 0 : failed_sends + 1; // If we don't get ack, increase number of failed sends
    if (failed_sends >= 5) { 
        memset(node_peer, 0, ESP_NOW_ETH_ALEN); // Clear peer address
        failed_sends = 0; // Clear failed sends
    }
}


/// @brief 
/// @param mac MAC address of the sender
/// @param packet The packet received
void node_rx_callback(const uint8_t *mac, const zenith_now_packet_t *packet)
{
    switch (packet->type)
    {
        case PACKET_ACK:
            switch (packet->ack_packet_type)
            {
                case PACKET_PAIRING:                    
                    memcpy(node_peer, mac, ESP_NOW_ETH_ALEN); // Store peer mac in RTC memory
                    break;
                case PACKET_DATA:
                    failed_sends = 0; // Extra handling of late ack. Need a bit of luck for this to trigger after the send times out and before the deep_sleep starts.
            }
    }
}

void app_main(void)
{
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause(); // Debug code to enable easy reflashing
    if (cause != ESP_SLEEP_WAKEUP_TIMER)
    {
        ESP_LOGI(TAG, "debug - Sleeping 30 sec for reflash purposes");
        vTaskDelay(pdMS_TO_TICKS(30000));
    }
    ESP_ERROR_CHECK(init_zenith_blink(GPIO_NUM_8)); // Initialize blink LED and abort if it fails
    ESP_ERROR_CHECK(configure_zenith_now(node_rx_callback, NULL)); // Set up zenith-now and abort if it fails
    if (!saved_peer()) // Check for paired core
        pair_with_core(); // Pair with core
    send_data(); // Send the data
    esp_deep_sleep(30*1000*1000); //Sleep for 30 seconds
}