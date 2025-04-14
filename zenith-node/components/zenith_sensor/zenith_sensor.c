#include <stdio.h>
#include "esp_check.h"
#include "zenith_sensor.h"

static const char *TAG = "zenith_sensor";

esp_err_t zenith_sensor_read_temperature(zenith_sensor_handle_t sensor, float *out_temp) {
    esp_err_t ret;
    ESP_RETURN_ON_FALSE(
        sensor && out_temp && sensor->read_temperature,
        ESP_ERR_INVALID_ARG,
        TAG, "Invalid arguments passed to read_temperature"
    );
    ret = sensor->read_temperature(sensor, out_temp);
    return ret;
}

esp_err_t zenith_sensor_read_humidity(zenith_sensor_handle_t sensor, float *out_humidity) {
    esp_err_t ret;
    ESP_RETURN_ON_FALSE(
        sensor && out_humidity && sensor->read_humidity,
        ESP_ERR_INVALID_ARG,
        TAG, "Invalid arguments passed to read humidity"
    );
    ret = sensor->read_humidity(sensor, out_humidity);
    return ret;
}

esp_err_t zenith_sensor_read_pressure(zenith_sensor_handle_t sensor, float *out_pressure) {
    esp_err_t ret;
    ESP_RETURN_ON_FALSE(
        sensor && out_pressure && sensor->read_pressure,
        ESP_ERR_INVALID_ARG,
        TAG, "Invalid arguments passed to pressure"
    );
    ret = sensor->read_pressure(sensor, out_pressure);
    return ret;
}