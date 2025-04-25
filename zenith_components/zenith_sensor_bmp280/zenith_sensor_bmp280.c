// zenith_sensor_bmp280.c
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include <stdio.h>
#include "esp_err.h"
#include "esp_check.h"
#include "string.h"

#include "zenith_sensor_bmp280.h"


static const char *TAG = "zenith_sensor_bmp280";
static int64_t t_fine;

esp_err_t zenith_sensor_bmp280_read_temperature(zenith_sensor_t *sensor, zenith_sensor_datatype_t *out_temp) {
    // to be implemented
    *out_temp = 25.0;
    return ESP_OK;
}

esp_err_t zenith_sensor_bmp280_read_pressure(zenith_sensor_t *sensor, zenith_sensor_datatype_t *out_pressure) {
    // to be implemented
    zenith_sensor_bmp280_read_temperature( sensor, out_pressure );
    *out_pressure = 1000.1;
    return ESP_OK;
}

esp_err_t _read_register( zenith_sensor_bmp280_t *bmp280, uint8_t reg, uint8_t *data, size_t datasize ) {
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(
        bmp280,
        ESP_ERR_INVALID_ARG,
        TAG, "NULL passed to _read_register"
    );

    ESP_RETURN_ON_ERROR(
        i2c_master_transmit_receive( bmp280->dev_handle, &reg, 1, data, datasize, portMAX_DELAY ),
        TAG, "Error reading BMP280 register 0x%x", reg
    );
    return ret;
}

esp_err_t _read_calibration_data( zenith_sensor_bmp280_t *bmp280 ) {
    esp_err_t ret = ESP_OK;

     ESP_RETURN_ON_ERROR(
        _read_register( bmp280, BMP280_REGISTER_DIG_T1, bmp280->calibration_data, sizeof( bmp280->calibration_data ) ),
        TAG, "Error reading calibration data"
    );
    
    return ret;
}

esp_err_t zenith_sensor_bmp280_initialize( zenith_sensor_t *sensor ) {
    esp_err_t ret = ESP_OK;
    
    zenith_sensor_bmp280_t *bmp280 = __containerof( sensor, zenith_sensor_bmp280_t, base );
    ESP_RETURN_ON_FALSE(
        bmp280,
        ESP_ERR_INVALID_ARG,
        TAG, "Invalid sensor handle"
    );

    ESP_RETURN_ON_ERROR(
        _read_calibration_data( bmp280 ),
        TAG, "Error initializing sensor"
    );

    bmp280->base.number_of_sensors = 2;
    ESP_LOGI( TAG, "bmp280 Sensor initialized" );
    return ret;
}

esp_err_t zenith_sensor_new_bmp280( i2c_master_bus_handle_t i2c_bus, zenith_sensor_bmp280_config_t *config, zenith_sensor_handle_t *handle) {
    esp_err_t ret = ESP_OK;

    ESP_RETURN_ON_FALSE(
        config && handle,
        ESP_ERR_INVALID_ARG,
        TAG, "Invalid arguments passed to new_aht30"
    );

    zenith_sensor_bmp280_handle_t bmp280 = calloc( 1, sizeof( zenith_sensor_bmp280_t ) ) ;
    ESP_RETURN_ON_FALSE(
        bmp280,
        ESP_ERR_NO_MEM,
        TAG, "Error allocating memory for sensor"
    );

    bmp280->bus_handle = i2c_bus;
    bmp280->bmp280_ctrl_meas = bmp280->config.control_measure;
    bmp280->bmp280_config = bmp280->bmp280_config; //??
    memcpy( &bmp280->config, config, sizeof( zenith_sensor_bmp280_config_t ) );

    i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = config->device_address,
        .scl_speed_hz = config->scl_speed_hz,
    };
    ESP_GOTO_ON_ERROR(
        i2c_master_bus_add_device( bmp280->bus_handle, &device_config, &bmp280->dev_handle ),
        err,
        TAG, "Error adding i2c device to bus"
    );

    bmp280->base.initialize = zenith_sensor_bmp280_initialize;
    bmp280->base.read_temperature = zenith_sensor_bmp280_read_temperature;
    bmp280->base.read_pressure = zenith_sensor_bmp280_read_pressure;

    *handle = &( bmp280->base );
    return ESP_OK;
err:
    if ( bmp280 )
        free( bmp280 );

    return ret;
}
