#include <stdio.h>
#include "esp_check.h"
#include "zenith_sensor.h"

static const char *TAG = "zenith_sensor";

esp_err_t zenith_sensor_read_temperature( zenith_sensor_handle_t sensor, float *out_temp ) {
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(
        sensor && out_temp,
        ESP_ERR_INVALID_ARG,
        TAG, "Invalid arguments passed to read_temperature"
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
        TAG, "Invalid arguments passed to read humidity"
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
        TAG, "Invalid arguments passed to pressure"
    );
    if ( sensor->read_pressure )
        ret = sensor->read_pressure( sensor, out_pressure );
    return ret;
}