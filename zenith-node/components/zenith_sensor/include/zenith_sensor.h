#pragma once

#include "esp_err.h"
#include "driver/i2c_master.h"
#include "driver/spi_common.h"

typedef enum zenith_sensor_type_e {
    SENSOR_ATH30,
    SENSOR_BMP280,
    SENSOR_MAX
} zenith_sensor_type_t;

typedef void * aht30_handle_t;

typedef union zenith_sensor_handle_u {
    bmp280_handle_t bmp280;
    aht30_handle_t  aht30;
} zenith_sensor_handle_t;

typedef union zenith_sensor_io_handle_u {

} zenith_sensor_io_handle_t;

typedef enum zenith_sensor_connection_type_e {
    CONNECTION_I2C,
    CONNECTION_SPI,
    CONNECTION_TYPE_MAX
} zenith_sensor_connection_type_t;

typedef struct zenith_sensor_config_s {
    zenith_sensor_type_t sensor_type;
    zenith_sensor_connection_type_t connection_type;
} zenith_sensor_config_t;

typedef struct zenith_sensor_s zenith_sensor_t;
typedef zenith_sensor_t *zenith_sensor_handle_t;

struct zenith_sensor_s {
    esp_err_t ( *init )( zenith_sensor_t *sensor );
    esp_err_t ( *read_temperature )( zenith_sensor_t *sensor );
    zenith_sensor_config_t config;
    zenith_sensor_io_handle_t io_handle;
};

esp_err_t zenith_sensor_create( zenith_sensor_config_t* config, zenith_sensor_handle_t* handle );