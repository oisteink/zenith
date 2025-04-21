// zenith_datapoints.c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "esp_err.h"
#include "esp_check.h"
#include "string.h"

#include "zenith_datapoints.h"
#include "zenith_sensor.h"
#include "zenith_now.h"

static const char *TAG = "zenith_nodes";

esp_err_t zenith_datapoints_add( zenith_datapoints_handle_t datapoints_handle, zenith_datapoint_t datapoint ) { 
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

// Bytestream is 
// 1 byte number of datapoints
// 1 byte datatype, then 4 bytes float
esp_err_t zenith_datapoints_from_zenith_now( const zenith_now_packet_t * in_packet, zenith_datapoints_handle_t *out_datapoints ) {
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(
        in_packet,
        ESP_ERR_INVALID_ARG,
        TAG, "NULL packet handle zenith_datapoints_from_zenith_now"
    );

    ESP_RETURN_ON_FALSE(
        out_datapoints == NULL,
        ESP_ERR_INVALID_ARG,
        TAG, "Datapoints should be NULL"
    );

    // hmm - om dette er lurt?
    if ( *out_datapoints == NULL ) 
        *out_datapoints = calloc( 1, sizeof( zenith_datapoints_t ) );

    zenith_datapoints_handle_t datapoints = calloc( 1, sizeof( zenith_datapoints_t ) );
    ESP_RETURN_ON_FALSE(
        datapoints,
        ESP_ERR_NO_MEM,
        TAG, "Failed to allocate memory for datapoints"
    );

    if ( datapoints->number_of_datapoints > 0 ) {
        // clean out current values 
        ESP_ERROR_CHECK(
            zentih_datapoints_clear( datapoints )
        );
    }
    
    zenith_now_payload_data_t *data_payload = ( zenith_now_payload_data_t * ) in_packet->payload;

    for ( int i = 0; i < data_payload->num_points; ++i ) {
        
        zenith_datapoint_t datapoint =
        {
            .data_type = data_payload->datapoints[i].reading_type,
            .data = data_payload->datapoints[i].value,
        };

        ret = zenith_datapoints_add(datapoints, datapoint);
        if ( ret != ESP_OK ) 
            break;
    }

    *out_datapoints = datapoints;
    return ret;
}


esp_err_t zenith_datapoints_to_zenith_now( const zenith_datapoints_handle_t datapoints, zenith_now_packet_handle_t *out_packet ) {
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(
        datapoints,
        ESP_ERR_INVALID_ARG,
        TAG, "NULL passed to zenith_nodes_data_serialize"
    );
    
    size_t payload_size = sizeof( zenith_now_payload_data_t ) + ( datapoints->number_of_datapoints * sizeof( zenith_node_datapoint_t ) );
    int packet_size = sizeof( zenith_now_packet_t ) + payload_size;
    ESP_LOGI( TAG, "datapoints -> zenith now, packet size: %d", packet_size );

    zenith_now_packet_t *packet = calloc( 1, packet_size );
    packet->header.type = ZENITH_PACKET_DATA;
    packet->header.version = ZENITH_NOW_VERSION;
    packet->header.payload_size = payload_size;

    zenith_now_payload_data_t *payload = ( zenith_now_payload_data_t * ) packet->payload;
    payload->num_points = datapoints->number_of_datapoints;

    zenith_datapoint_node_t *node = datapoints->datapoint_nodes;
    for ( int i = 0; i < datapoints->number_of_datapoints; ++i ) {
        if ( node == NULL ) {
            ret = ESP_ERR_INVALID_STATE;
            break;
        }
        payload->datapoints[i].reading_type = node->datapoint.data_type;
        payload->datapoints[i].value = node->datapoint.data;
        ESP_LOGI( TAG, "node %d: %d | %.2f", i, node->datapoint.data_type, node->datapoint.data );
        ESP_LOGI( TAG, "node %d: %d | %d", i, payload->datapoints[i].reading_type, payload->datapoints[i].value );
        node = node->next;
    }

    *out_packet = packet;
    return ret;
}

esp_err_t zentih_datapoints_clear(zenith_datapoints_handle_t datapoints_handle) {
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(
        datapoints_handle,
        ESP_ERR_INVALID_ARG,
        TAG, "NULL handle passed to zentih_datapoint_clean"
    );

    zenith_datapoint_node_t *current = datapoints_handle->datapoint_nodes;
    zenith_datapoint_node_t *next = NULL;

    // remove all datapoint nodes
    while (current != NULL) {
        next = current->next;  
        free(current);         
        current = next;        
    }

    // Reset the list head and count
    datapoints_handle->datapoint_nodes = NULL;
    datapoints_handle->number_of_datapoints = 0;

    return ret;
}