// zenith_datapoints.c
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "esp_err.h"
#include "esp_check.h"
#include "string.h"

#include "zenith_data.h"
#include "zenith_sensor.h"
#include "zenith_now.h"

static const char *TAG = "zenith_nodes";

int zenith_datapoints_calculate_size( uint8_t num_datapoints ) {
    // Calculate the size of the datapoints structure
    int size = sizeof( zenith_datapoints_t ) + sizeof( zenith_datapoint_t ) * num_datapoints;
    ESP_LOGD(TAG, "Datapoints size: %d", size);
    return size;   
}

esp_err_t zenith_datapoints_new( zenith_datapoints_handle_t *datapoints_handle, uint8_t num_datapoints ) {
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE( 
        datapoints_handle,
        ESP_ERR_INVALID_ARG, 
        TAG, "datapoints_handle is NULL" 
    );

    ESP_LOGD(TAG, "Creating new datapoints handle with %d datapoints", num_datapoints);

    // Allocate memory for the datapoints structure
    zenith_datapoints_handle_t new_datapoints = (zenith_datapoints_handle_t) calloc(1, zenith_datapoints_calculate_size( num_datapoints ) );
    ESP_RETURN_ON_FALSE(
        new_datapoints,
        ESP_ERR_NO_MEM,
        TAG, "Failed to allocate memory for datapoints"
    );
    ESP_LOGD(TAG, "Allocated memory for datapoints handle");

    // Initialize the number of datapoints
    new_datapoints->num_datapoints = num_datapoints;

    // Set the output parameter
    *datapoints_handle = new_datapoints;

    return ret;
}

