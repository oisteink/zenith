#pragma once

#include <stdint.h>
#include "esp_err.h"

typedef struct zenith_sensor_s zenith_sensor_t;
typedef zenith_sensor_t *zenith_sensor_handle_t;

struct zenith_sensor_s {
    esp_err_t ( *read_temperature )( zenith_sensor_t *sensor, float *out_temp ); // Temperature is scaled by 100
    esp_err_t ( *read_humidity )( zenith_sensor_t *sensor, float *out_humidity ); // Humidity is scaled by 100
    esp_err_t ( *read_pressure )( zenith_sensor_t *sensor, float *out_pressure ); // Rounded to integer
};

esp_err_t zenith_sensor_read_temperature(zenith_sensor_handle_t sensor, float *out_temp);
esp_err_t zenith_sensor_read_humidity(zenith_sensor_handle_t sensor, float *out_humidity);
esp_err_t zenith_sensor_read_pressure(zenith_sensor_handle_t sensor, float *out_pressure);