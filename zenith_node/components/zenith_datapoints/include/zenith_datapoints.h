#pragma once

typedef enum zenith_datapoints_datatype_e {
    ZENITH_DATAPOINT_TEMPERATURE,
    ZENITH_DATAPOINT_HUMIDITY,
    ZENITH_DATAPOINT_PRESSURE,
    _ZENITH_DATAPOINT_MAX
} zenith_datapoints_datatype_t;

typedef struct zenith_datapoint_s {
    zenith_datapoints_datatype_t data_type;
    union {
        float data;
    };
} zenith_datapoint_t;

typedef struct zenith_datapoint_node_s zenith_datapoint_node_t;

struct zenith_datapoint_node_s {
    zenith_datapoint_t datapoint;
    zenith_datapoint_node_t *next;
};

typedef struct zenith_datapoints_s {
    uint8_t number_of_datapoints;
    zenith_datapoint_node_t *datapoint_nodes;
} zenith_datapoints_t;

typedef zenith_datapoints_t *zenith_datapoints_handle_t;

esp_err_t zenith_datapoint_add( zenith_datapoints_handle_t datapoints_handle, zenith_datapoint_t datapoint );
esp_err_t zenith_datapoints_deserialize( const zenith_now_sensor_data_t *bytestream, zenith_datapoints_t *datapoints );
esp_err_t zenith_datapoint_serialize( const zenith_datapoints_t *sensor_data, zenith_now_sensor_data_t *bytestream );