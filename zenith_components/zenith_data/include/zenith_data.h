// zenith_data.h

#pragma once
#include "zenith_now.h"

typedef enum zenith_datapoints_datatype_e {
    ZENITH_DATAPOINT_TEMPERATURE,
    ZENITH_DATAPOINT_HUMIDITY,
    ZENITH_DATAPOINT_PRESSURE,
    _ZENITH_DATAPOINT_MAX
} zenith_datapoints_datatype_t;

typedef uint16_t zenith_sensor_datatype_t;

/// @brief Zenith Now node datapoint.
/// @details This structure is used to represent a single data point from a node. It contains the value of the reading and the type of the reading.
typedef struct __attribute__((packed)) zenith_datapoint_s {
    /// @brief The type of the datapoint.
    /// @details This is a 1-byte value that indicates the type of datapoint.
    uint8_t reading_type;
    /// @brief The value of the datapoint.
    /// @details This is a 2-byte value that represents the actual datapoint.
    zenith_sensor_datatype_t value;      
} zenith_datapoint_t; // zenith_now_data_datapoint_t? or general node datapoint? 
///@note I can use this format in memory for the core as well. I do not need to alter or add measurements - they are historic data the second they arrive

/// @brief Zenith Now data sensor data
/// @details This structure is used to represent a data packet that contains multiple datapoints. The number of datapoints is specified in the num_points field.
typedef struct __attribute__((packed)) zenith_datapoints_s {
    uint8_t num_datapoints;
    zenith_datapoint_t datapoints[];
} zenith_datapoints_t;

typedef zenith_datapoints_t *zenith_datapoints_handle_t;

esp_err_t zenith_datapoints_new( zenith_datapoints_handle_t *datapoints_handle, uint8_t num_datapoints );
int zenith_datapoints_calculate_size( uint8_t num_datapoints );