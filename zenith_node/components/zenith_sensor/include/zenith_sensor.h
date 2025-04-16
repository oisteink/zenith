#pragma once

#include <stdint.h>
#include "esp_err.h"

typedef struct zenith_sensor_s zenith_sensor_t;
typedef zenith_sensor_t *zenith_sensor_handle_t;


struct zenith_sensor_s {
    esp_err_t ( *initialize )( zenith_sensor_t *sensor ); // Called to initialize the sensor
    esp_err_t ( *read_temperature )( zenith_sensor_t *sensor, float *out_temp ); 
    esp_err_t ( *read_humidity )( zenith_sensor_t *sensor, float *out_humidity );
    esp_err_t ( *read_pressure )( zenith_sensor_t *sensor, float *out_pressure );
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

typedef enum zenith_sensor_datatype_e {
    ZENITH_SENSOR_DATATYPE_TEMPERATURE,
    ZENITH_SENSOR_DATATYPE_HUMIDITY,
    ZENITH_SENSOR_DATATYPE_PRESSURE,
    ZENITH_SENSOR_DATATYPE_MAX
} zenith_sensor_datatype_t;

typedef struct zenith_sensor_datapoint_s {
    zenith_sensor_datatype_t data_type;
    union {
        float data;
    }
} zenith_sensor_datapoint_t;

typedef struct zenith_sensor_datapoint_node_s zenith_sensor_datapoint_node_t;

struct zenith_sensor_datapoint_node_s {
    zenith_sensor_datapoint_t datapoint;
    zenith_sensor_datapoint_node_t *next;
};

typedef struct zenith_sensor_data_s {
    uint8_t number_of_datapoints;
    zenith_sensor_datapoint_node_t *datapoint_nodes;
} zenith_sensor_data_t;

esp_err_t zenith_sensor_data_add_datapoint( zenith_sensor_data_t *sensor_data, zenith_sensor_datapoint_t datapoint );

esp_err_t zentih_sensor_init( zenith_sensor_handle_t sensor );
esp_err_t zenith_sensor_read_data( zenith_sensor_handle_t sensor, zenith_sensor_data_t *data );
esp_err_t zenith_sensor_read_temperature( zenith_sensor_handle_t sensor, float *out_temp );
esp_err_t zenith_sensor_read_humidity( zenith_sensor_handle_t sensor, float *out_humidity );
esp_err_t zenith_sensor_read_pressure( zenith_sensor_handle_t sensor, float *out_pressure );