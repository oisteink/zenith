#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "zenith_data.h"

typedef struct zenith_sensor_s zenith_sensor_t;
typedef zenith_sensor_t *zenith_sensor_handle_t;

struct zenith_sensor_s {
    esp_err_t ( *initialize )( zenith_sensor_handle_t sensor ); // Called to initialize the sensor
    esp_err_t ( *read_temperature )( zenith_sensor_handle_t sensor, zenith_sensor_datatype_t *out_temp ); 
    esp_err_t ( *read_humidity )( zenith_sensor_handle_t sensor, zenith_sensor_datatype_t *out_humidity );
    esp_err_t ( *read_pressure )( zenith_sensor_handle_t sensor, zenith_sensor_datatype_t *out_pressure );
    //esp_err_t ( *read_data )( zenith_sensor_handle_t sensor, zenith_datapoints_t *datapoints );
    uint8_t number_of_sensors;
};

struct zenith_sensor_float_s {
    
    float temperature;
};

struct zenith_sensor_pressure_s {
    float pressure;
};

struct zenith_sensor_humidity_s {
    float humidity;
};


esp_err_t zentih_sensor_init( zenith_sensor_handle_t sensor );
esp_err_t zenith_sensor_read_data( zenith_sensor_handle_t sensor, zenith_datapoints_handle_t *datapoints );
esp_err_t zenith_sensor_read_temperature( zenith_sensor_handle_t sensor, zenith_sensor_datatype_t *out_temp );

esp_err_t zenith_sensor_read_humidity( zenith_sensor_handle_t sensor, zenith_sensor_datatype_t *out_humidity );
esp_err_t zenith_sensor_read_pressure( zenith_sensor_handle_t sensor, zenith_sensor_datatype_t *out_pressure );