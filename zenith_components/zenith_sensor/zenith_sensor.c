//zenith_sensor.c
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

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

esp_err_t zenith_sensor_read_data( zenith_sensor_handle_t sensor, zenith_datapoints_t **datapoints ) {
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(
        sensor && datapoints,
        ESP_ERR_INVALID_ARG,
        TAG, "NULL pointer passed to zenith_sensor_read_data"
    );

    zenith_datapoints_t *data = NULL;
    zenith_datapoints_new( &data, sensor->number_of_sensors );
    ESP_RETURN_ON_FALSE(
        data,
        ESP_ERR_NO_MEM,
        TAG, "Failed to allocate memory for datapoints"
    );

    uint8_t current_datapoint = 0;
    if ( sensor->read_humidity )
    {
        data->datapoints[current_datapoint].reading_type = ZENITH_DATAPOINT_HUMIDITY;
        zenith_sensor_datatype_t humidity;
        ret = sensor->read_humidity( sensor, &humidity );
        if ( ret == ESP_OK ) {
            data->datapoints[current_datapoint].value = humidity;
            current_datapoint++;
        }
    }
    
    if ( sensor->read_pressure )
    {
        data->datapoints[current_datapoint].reading_type = ZENITH_DATAPOINT_PRESSURE;
        zenith_sensor_datatype_t pressure;
        ret = sensor->read_pressure( sensor, &pressure );
        if ( ret == ESP_OK ) {
            data->datapoints[current_datapoint].value = pressure;
            current_datapoint++;
        }
    }

    if ( sensor->read_temperature )
    {
        data->datapoints[current_datapoint].reading_type = ZENITH_DATAPOINT_TEMPERATURE;
        zenith_sensor_datatype_t temperature;
        ret = sensor->read_temperature( sensor, &temperature );
        if ( ret == ESP_OK ) {
            data->datapoints[current_datapoint].value = temperature;
            current_datapoint++;
        }
    }

    *datapoints = data;
    return ret;
}

esp_err_t zenith_sensor_read_temperature( zenith_sensor_handle_t sensor, zenith_sensor_datatype_t *out_temp ) {
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

esp_err_t zenith_sensor_read_humidity( zenith_sensor_handle_t sensor, zenith_sensor_datatype_t *out_humidity ) {
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

esp_err_t zenith_sensor_read_pressure( zenith_sensor_handle_t sensor, zenith_sensor_datatype_t *out_pressure ) {
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