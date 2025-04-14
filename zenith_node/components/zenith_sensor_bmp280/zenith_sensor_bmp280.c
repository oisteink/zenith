#include <stdio.h>
#include "esp_err.h"
#include "esp_check.h"
#include "string.h"

#include "zenith_sensor_bmp280.h"


static const char *TAG = "zenith_sensor_bmp280";
static int64_t t_fine;

esp_err_t _read_temperature(zenith_sensor_t *sensor, float *out_temp)
{
    // to be implemented
    *out_temp = 0.0;
    return ESP_OK;
}

esp_err_t _read_pressure(zenith_sensor_t *sensor, float *out_pressure)
{
    // to be implemented
    _read_temperature(sensor, out_pressure);
    *out_pressure = 0.0;
    return ESP_OK;
}

esp_err_t zenith_sensor_new_bmp280( i2c_master_bus_handle_t i2c_bus, zenith_sensor_bmp280_config_t *config, zenith_sensor_handle_t *handle)
{
    esp_err_t ret = ESP_OK;

    ESP_RETURN_ON_FALSE(
        config && handle,
        ESP_ERR_INVALID_ARG,
        TAG, "Invalid arguments passed to new_aht30"
    );

    zenith_sensor_bmp280_handle_t bmp280 = calloc( 1, sizeof( zenith_sensor_bmp280_t )) ;
    ESP_RETURN_ON_FALSE(
        bmp280,
        ESP_ERR_NO_MEM,
        TAG, "Error allocating memory for sensor"
    );

    bmp280->bus_handle = i2c_bus;
    memcpy( &bmp280->config, config, sizeof( zenith_sensor_bmp280_config_t ) );

    i2c_device_config_t device_config = {
        .dev_addr_length = 7,
        .device_address = config->device_address,
        .scl_speed_hz = config->scl_speed_hz,
    };
    ESP_GOTO_ON_ERROR(
        i2c_master_bus_add_device(bmp280->bus_handle, &device_config, &bmp280->dev_handle),
        err,
        TAG, "Error adding i2c device to bus"
    );

    bmp280->base.read_temperature = _read_temperature;
    bmp280->base.read_pressure = _read_pressure;

    *handle = &(bmp280->base);
    return ESP_OK;
err:
    if (bmp280)
        free(bmp280);

    return ret;
}
