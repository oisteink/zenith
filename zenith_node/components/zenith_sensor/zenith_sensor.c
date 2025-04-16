#include <stdio.h>
#include "esp_check.h"
#include "string.h"
#include "zenith_sensor.h"

static const char *TAG = "zenith_sensor";

esp_err_t zentih_sensor_init( zenith_sensor_handle_t sensor ) {
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(
        sensor,
        ESP_ERR_INVALID_ARG,
        TAG, "NULL pointer passed to init"
    );
    if ( sensor->initialize )
        ret = sensor->initialize( sensor );
    return ret;
}

// my head is exploding! 
esp_err_t zenith_sensor_data_add_datapoint( zenith_sensor_data_t *sensor_data, zenith_sensor_datapoint_t datapoint ) { // Pass sensor_data by reference
    if ( !sensor_data ) 
        return ESP_ERR_INVALID_ARG;

    zenith_sensor_datapoint_node_t **tail = &sensor_data->datapoint_nodes; // point tail at the pointer for data_nodes
    while ( *tail ) // As long as what it points to isn't NULL
        tail = &(*tail)->next; // Dereference tail, and point to a pointer (&) to it's next (pointer)

    *tail = calloc(1, sizeof( zenith_sensor_datapoint_node_t ) ); // We've reached the first pointer that points to NULL, so we add the new node here.
    if ( !*tail )
        return ESP_ERR_NO_MEM;

    (*tail)->datapoint = datapoint; // Copy the data over
    sensor_data->number_of_datapoints++; // Increase the count. 

    return ESP_OK;          
}


/* esp_err_t zenith_sensor_read_data( zenith_sensor_handle_t sensor, zenith_sensor_data_t *sensor_data ) {
    esp_err_t ret = ESP_OK;
    zenith_sensor_data_t *data = calloc( 1, sizeof( zenith_sensor_data_t ) );

    sensor_data = data;
    return ret;
}
 */
esp_err_t zenith_sensor_read_temperature( zenith_sensor_handle_t sensor, float *out_temp ) {
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(
        sensor && out_temp,
        ESP_ERR_INVALID_ARG,
        TAG, "NULL pointer passed to read_temperature"
    );
    if ( sensor->read_temperature )
        ret = sensor->read_temperature( sensor, out_temp );
    return ret;
}

esp_err_t zenith_sensor_read_humidity( zenith_sensor_handle_t sensor, float *out_humidity ) {
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(
        sensor && out_humidity,
        ESP_ERR_INVALID_ARG,
        TAG, "NULL pointer passed to read humidity"
    );

    if ( sensor->read_humidity )
        ret = sensor->read_humidity (sensor, out_humidity );
    return ret;
}

esp_err_t zenith_sensor_read_pressure( zenith_sensor_handle_t sensor, float *out_pressure ) {
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(
        sensor && out_pressure,
        ESP_ERR_INVALID_ARG,
        TAG, "NULL pointer passed to pressure"
    );
    if ( sensor->read_pressure )
        ret = sensor->read_pressure( sensor, out_pressure );
    return ret;
}