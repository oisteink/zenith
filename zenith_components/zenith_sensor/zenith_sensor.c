#include <stdio.h>
#include "esp_err.h"
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

esp_err_t zenith_sensor_read_data( zenith_sensor_handle_t sensor, zenith_datapoints_handle_t datapoints ) {
    esp_err_t ret;
    ESP_RETURN_ON_FALSE(
        sensor || datapoints,
        ESP_ERR_INVALID_ARG,
        TAG, "NULL pointer passed to zenith_sensor_read_data"
    );

    zenith_datapoint_t datapoint = { 0 };

    if ( sensor->read_humidity )
    {
        datapoint.data_type = ZENITH_DATAPOINT_HUMIDITY;
        ret = sensor->read_humidity( sensor, &datapoint.data);
        if ( ret == ESP_OK )
            ret = zenith_datapoint_add( datapoints, datapoint );
    }
    
    if ( sensor->read_pressure )
    {
        datapoint.data_type = ZENITH_DATAPOINT_PRESSURE;
        ret = sensor->read_pressure( sensor, &datapoint.data);
        if ( ret == ESP_OK )
            ret = zenith_datapoint_add( datapoints, datapoint );
    }

    if ( sensor->read_temperature )
    {
        datapoint.data_type = ZENITH_DATAPOINT_TEMPERATURE;
        ret = sensor->read_temperature( sensor, &datapoint.data);
        if ( ret == ESP_OK )
            ret = zenith_datapoint_add( datapoints, datapoint );
    }
    return ret;
}

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