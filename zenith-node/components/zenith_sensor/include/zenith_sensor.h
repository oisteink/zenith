#pragma once

#include <stdint.h>
#include "esp_err.h"

typedef struct zenith_sensor_s zenith_sensor_t;
typedef zenith_sensor_t *zenith_sensor_handle_t;

struct zenith_sensor_s {
    esp_err_t ( *read_temperature )( zenith_sensor_t *sensor, int16_t *out_temp ); // Temperature is scaled by 100
    esp_err_t ( *read_humidity )( zenith_sensor_t *sensor, int16_t *out_humidity ); // Humidity is scaled by 100
    esp_err_t ( *read_pressure )( zenith_sensor_t *sensor, int16_t *out_pressure ); // Rounded to integer
};