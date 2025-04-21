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
#include "esp_sleep.h"

#include "zenith_now.h"
#include "zenith_blink.h"

#include "zenith_node.h"
#include "zenith_datapoints.h"

#include "zenith_sensor_aht30.h"
#include "zenith_sensor_bmp280.h"
#include "zenith_sensor.h"

/* zenith specific variables */
static const char *TAG = "zenith-node";
RTC_DATA_ATTR static uint8_t paired_core[ ESP_NOW_ETH_ALEN ] = { 0 }; // peers mac address
RTC_DATA_ATTR uint8_t failed_sends = 0;


bool saved_peer( void ){
    uint8_t empty_mac[ ESP_NOW_ETH_ALEN ] = { 0 };
    return ( memcmp( paired_core, empty_mac, ESP_NOW_ETH_ALEN ) != 0 );
}

/// @brief  Enter pairingmode
/// @return Allways returns ESP_OK
void pair_with_core( void ){
    // Add broadcast peer
    uint8_t broadcast[ ESP_NOW_ETH_ALEN ] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
    ESP_ERROR_CHECK(
        zenith_now_add_peer( broadcast ) 
    ); 

    // Create pairing packet
    zenith_now_packet_t data_packet = { 
        .type = ZENITH_PACKET_PAIRING
    }; 

    // initialize counter for pairing retries
    uint8_t peering_tries = 0; 
    do {
        // If we miss 5 pairing requests, enter deeps sleep
        if (peering_tries++ >= 5)
            esp_deep_sleep(NODE_SLEEP_NO_PEER); 
        
        // Blink to show we are trying to pair
        ESP_ERROR_CHECK(
            zenith_blink( BLINK_PAIRING )
        ); 

        // Send the pairing request
        ESP_ERROR_CHECK(
            zenith_now_send_packet( broadcast, data_packet )
        ); 
    } while ( zenith_now_wait_for_ack( ZENITH_PACKET_PAIRING, 5000 ) != ESP_OK ); // Wait 5 seconds for ack, and retry if we timed out

    // Remove broadcast peer
    ESP_ERROR_CHECK(
        zenith_now_remove_peer( broadcast )
    ); 

    // Wait for paired_core to be stored. 
    while ( ! saved_peer() )  
        vTaskDelay( pdMS_TO_TICKS( 50 ) );
}

 esp_err_t i2c_init(i2c_master_bus_handle_t *i2c_bus){
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_MASTER_NUM,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(
        i2c_new_master_bus( &bus_config, i2c_bus )
    );

    return ESP_OK;
}

#define BMP280_SENSOR

esp_err_t init_sensor( zenith_sensor_handle_t *sensor, i2c_master_bus_handle_t i2c_bus ){
#ifdef BMP280_SENSOR
    zenith_sensor_bmp280_config_t bmp280_config = DEFAULT_ZENITH_SENSOR_BMP280_CONFIG;
    ESP_RETURN_ON_ERROR(
        zenith_sensor_new_bmp280( i2c_bus, &bmp280_config, sensor),
        TAG, "Error creating sensor"
    );   
    ESP_LOGI(TAG, "Created bmp280 sensor");
#else
    zenith_sensor_aht30_config_t aht30_config = DEFAULT_ZENITH_SENSOR_AHT30_CONFIG;
    ESP_RETURN_ON_ERROR(
        zenith_sensor_new_aht30( i2c_bus, &aht30_config, sensor ),
        TAG, "Error creating sensor"
    );
#endif
     ESP_RETURN_ON_ERROR(
        zentih_sensor_init( *sensor ),
        TAG, "Error initializing sensor"
    );

    return ESP_OK;
}

/// @brief Reads the sensor and sends the data to the paired_core
/// @return Allways returns ESP_OK
void send_data( zenith_sensor_handle_t sensor ){
    
    // Initialize data packet
     zenith_now_packet_t * data_packet = NULL;

    zenith_datapoints_t sensor_data = { 0 };
    ESP_ERROR_CHECK( 
        zenith_sensor_read_data( sensor, &sensor_data )
    );
    
    ESP_LOGI( TAG, "%d sensor data read", sensor_data.number_of_datapoints );

    // check this one!!
    zenith_datapoints_to_zenith_now( &sensor_data, &data_packet );
    zenith_now_payload_data_t *data = (zenith_now_payload_data_t *) data_packet->payload;
    ESP_LOGI( TAG, "%d sensor data converted to zenith now packet", data->num_points );
    for ( int i = 0; i < data->num_points; i++ ) {
        ESP_LOGI( TAG, "Sensor %d dat: %d | %d", i, data->datapoints[i].value, data->datapoints[i].reading_type );
    }
    // Healing: Ensure peer is in our list of peers
    ESP_ERROR_CHECK( 
        zenith_now_add_peer( paired_core ) 
    );

    // Send data
    ESP_ERROR_CHECK( 
        zenith_now_send_packet( paired_core, *data_packet ) 
    ); 
    // If we don't get ack, increase number of failed sends
    failed_sends = ( zenith_now_wait_for_ack( ZENITH_PACKET_DATA, 2000 ) == ESP_OK ) ? 0 : failed_sends + 1; 

    // On 5 failed sends we forget our peer
    if ( failed_sends >= 5 ) { 
        memset( paired_core, 0, ESP_NOW_ETH_ALEN ); // Clear peer address
        failed_sends = 0; // Clear failed sends
    }
}


/// @brief 
/// @param mac MAC address of the sender
/// @param packet The packet received
void node_rx_callback(const uint8_t *mac, const zenith_now_packet_t *packet){
    switch (packet->type) {
        case ZENITH_PACKET_ACK:
            zenith_now_payload_ack_t *ack = (zenith_now_payload_ack_t *) packet->payload;
            switch ( ack->ack_for_type ) {

                case ZENITH_PACKET_PAIRING:                    
                    memcpy( paired_core, mac, ESP_NOW_ETH_ALEN ); // Store peer mac in RTC memory
                    break;

                case ZENITH_PACKET_DATA:
                    failed_sends = 0; // Extra handling of late ack. Need a bit of luck for this to trigger after the send times out and before the deep_sleep starts.
                    break;
            }
            break;
        default:
            ESP_LOGI( TAG, "node_rx_cb: unhandled packet type %d", packet->type );
            break;
    }
}
 

void app_main( void ){
    // Debug code to enable easy reflashing
    if ( esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_TIMER )
    {
        ESP_LOGI( TAG, "debug - Sleeping 30 sec for reflash purposes" );
        vTaskDelay( pdMS_TO_TICKS( 30000 ) );
    }

    i2c_master_bus_handle_t i2c_bus = NULL;
    i2c_init( &i2c_bus );
    // Initialize sensor
    zenith_sensor_handle_t sensor = NULL;
    ESP_ERROR_CHECK( 
        init_sensor( &sensor, i2c_bus ) 
    );

    // Initialize blink LED
    ESP_ERROR_CHECK( 
        init_zenith_blink( GPIO_NUM_8 ) 
    ); 

    // Set up zenith-now
    ESP_ERROR_CHECK( 
        configure_zenith_now() 
    ); 
    zenith_now_set_rx_cb( node_rx_callback );

    // Check for paired core and pair if not
    if ( !saved_peer() ) 
        pair_with_core(); 

    // Send sensor data
    send_data( sensor ); 

    // Enter deep sleep
    esp_deep_sleep( 30 * 1000 * 1000 );  // DEBUG! 30 seconds
}