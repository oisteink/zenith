#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "string.h"
#include "zenith_sensor_aht30.h"

static const char *TAG = "AHT30";

esp_err_t _read_signal( zenith_sensor_aht30_t *aht30, uint8_t *data) {
    uint8_t command[] = {0xAC, 0x33, 0x00}; //   Wait 10ms to send the 0xAC command (trigger measurement). This command parameter has two bytes, the first byte is 0x33, and the second byte is 0x00.
    uint8_t tries = 0;
    do // Loop until ready: "If the status bit [Bit7] is 0, it means that the data can be read normaly. When it is 1, the sensor is busy, and the host needs to wait for the data processing to complete."
    {   
        tries++;
        vTaskDelay( pdMS_TO_TICKS( 10 ) );

        ESP_RETURN_ON_ERROR(
            i2c_master_transmit( aht30->dev_handle, command, sizeof( command ), AHT30_SENSOR_TIMEOUT ),
            TAG, "Error sending command to sensor"
        );

        vTaskDelay( pdMS_TO_TICKS( 80 ) ); // Wait 80ms for the measurement to be completed

        ESP_RETURN_ON_ERROR(
            i2c_master_receive( aht30->dev_handle, data, 6, AHT30_SENSOR_TIMEOUT ),
            TAG, "Error receiving data from sensor"
        );

        ESP_RETURN_ON_FALSE(
            tries < 5,
            ESP_ERR_INVALID_RESPONSE,
            TAG, "Sensor busy for > 500 ms"
        );
    } while ( data[0] & 0x80 ); // Loop until statusflag is clear
    return ESP_OK;
}

esp_err_t _read_temperature( zenith_sensor_t *sensor, zenith_sensor_datatype_t *out_temp )
{
    zenith_sensor_aht30_t *aht30 = __containerof(sensor, zenith_sensor_aht30_t, base);

    uint8_t data[6]; // 1 byte status, 5 bytes Humidity and Temperature signal
    ESP_RETURN_ON_ERROR(
        _read_signal(aht30, data),
        TAG, "Failed to get signal from sensor"
    );
    // Temperatur signal is lower 20 bits
    float signal = ((data[3] & 0x0F) << 16) | (data[4] << 8) | data[5];
    // formula in spec: ( signal / 2^20 ) * 200 - 50 = °C
    *out_temp =   signal / 1048576.0f * 200.0f - 50.0f; 

    return ESP_OK;
}

esp_err_t _read_humidity( zenith_sensor_t *sensor, zenith_sensor_datatype_t *out_humidity )
{
    zenith_sensor_aht30_t *aht30 = __containerof(sensor, zenith_sensor_aht30_t, base);

    uint8_t data[6]; // 1 byte status, 5 bytes Humidity and Temperature signal
    ESP_RETURN_ON_ERROR(
        _read_signal(aht30, data),
        TAG, "Failed to get signal from sensor"
    );
    // Humidity signal is upper 20 bits
    float signal = (data[1] << 12) | (data[2] << 4) | (data[3] >> 4);
    // formula in spec: ( signal / 1048576 ) * 100 = % RH
    *out_humidity =  signal / 1048576.0f * 100.0f; 

    return ESP_OK;
}


esp_err_t zenith_sensor_new_aht30( i2c_master_bus_handle_t i2c_bus, zenith_sensor_aht30_config_t *config, zenith_sensor_handle_t *handle)
{
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(
        config && handle,
        ESP_ERR_INVALID_ARG,
        TAG, "Invalid arguments passed to new_aht30"
    );

    zenith_sensor_aht30_handle_t aht30 = NULL;
    aht30 = calloc( 1, sizeof( zenith_sensor_aht30_t ) );
    ESP_RETURN_ON_FALSE(
        aht30,
        ESP_ERR_NO_MEM,
        TAG, "Error allocating memory for driver"
    );
    
    aht30->bus_handle = i2c_bus;
    memcpy( &aht30->config, config, sizeof( zenith_sensor_aht30_config_t ) );
    
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = aht30->config.device_address,
        .scl_speed_hz = aht30->config.scl_speed_hz,
    };
    ESP_GOTO_ON_ERROR(
        i2c_master_bus_add_device(aht30->bus_handle, &dev_config, &aht30->dev_handle),
        err,
        TAG, "Error adding i2c device to bus"
    );

    vTaskDelay( pdMS_TO_TICKS( 100 ) ); // After power-on, wait for ≥100ms Before reading the temperature and humidity value
    
    aht30->base.read_humidity = _read_humidity;
    aht30->base.read_temperature = _read_temperature;

    *handle = &(aht30->base);
    return ESP_OK;

err:
    if ( aht30 )
        free ( aht30 );
    return ret;
}