#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "esp_err.h"
#include "esp_check.h"
#include "string.h"

#include "zenith_datapoints.h"
#include "zenith_sensor.h"
#include "zenith_now.h"

static const char *TAG = "zenith_nodes";

esp_err_t zenith_datapoint_add( zenith_datapoints_handle_t datapoints_handle, zenith_datapoint_t datapoint ) { 
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(
        datapoints_handle,
        ESP_ERR_INVALID_ARG,
        TAG, "NULL handle passed to zenith_datapoint_add"
    );

    zenith_datapoint_node_t **tail = &datapoints_handle->datapoint_nodes; 
    while ( *tail ) 
        tail = &( *tail )->next;

    *tail = calloc( 1, sizeof( zenith_datapoint_node_t ) ); 
    if ( !*tail )
        return ESP_ERR_NO_MEM;

    ( *tail )->datapoint = datapoint; 
    datapoints_handle->number_of_datapoints++; 

    return ret;          
}

esp_err_t zenith_datapoints_deserialize( const zenith_now_sensor_data_t *bytestream, zenith_datapoints_t *datapoints ) {
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(
        bytestream || datapoints,
        ESP_ERR_INVALID_ARG,
        TAG, "NULL passed to zenith_datapoints_deserialize"
    );

    const uint8_t *ptr = bytestream->data_buffer;

    for ( int i = 0; i < bytestream->num_points; ++i ) {
        ESP_RETURN_ON_FALSE(
            bytestream->data_buffer + ZENITH_NOW_MAX_DATA_LEN > ptr + sizeof( uint8_t ) + sizeof( float ),
            ESP_ERR_INVALID_SIZE,
            TAG, "Whoa! we've ran out of bytestream while deserializing"
        );

        zenith_datapoint_t datapoint;
        datapoint.data_type = ( zenith_datapoints_datatype_t ) *ptr++;
        memcpy( &datapoint.data, ptr, sizeof( float ) );
        ptr += sizeof( float );

        ret = zenith_datapoint_add(datapoints, datapoint);
        if ( ret != ESP_OK ) 
            break;
    }

    return ret;
}


esp_err_t zenith_datapoint_serialize( const zenith_datapoints_t *sensor_data, zenith_now_sensor_data_t *bytestream ) {
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(
        sensor_data || bytestream,
        ESP_ERR_INVALID_ARG,
        TAG, "NULL passed to zenith_nodes_data_serialize"
    );
    
    uint8_t *ptr = bytestream->data_buffer;
    int remaining = ZENITH_NOW_MAX_DATA_LEN;
    zenith_datapoint_node_t *node = sensor_data->datapoint_nodes;

    bytestream->num_points = 0;

    while ( node && remaining >= sizeof( uint8_t ) + sizeof( float )) {
        *ptr++ = ( uint8_t ) node->datapoint.data_type;
        memcpy( ptr, &node->datapoint.data, sizeof( float ));
        ptr += sizeof( float );
        remaining -= ( sizeof( uint8_t ) + sizeof( float ));
        bytestream->num_points++;
        node = node->next;
    }

    return ret;
}