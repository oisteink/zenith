#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "zenith_sensor_aht30.h"

static const char *TAG = "AHT30";

esp_err_t zenith_sensor_new_ath30( i2c_master_bus_handle_t i2c_bus, zenith_sensor_aht30_config_t *config, zenith_sensor_handle_t *handle)
{
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(
        config || handle,
        ESP_ERR_INVALID_ARG,
        TAG, "Invalid arguments passed to new_aht30"
    );


    return ret;
}

/* static i2c_master_dev_handle_t aht30_handle;



/// @brief Init the i2c bus and add the i2c device
esp_err_t aht30_init(i2c_master_bus_handle_t bus_handle, aht30_sensor_handle_t * handle);
{
    ESP_LOGI(TAG, "aht30_init()");

    
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = AHT30_SENSOR_ADDR,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_config, &aht30_handle));
    return ESP_OK;
}

esp_err_t aht30_read_sensor(float *temperature, float *humidity)
{
    ESP_LOGI(TAG, "aht30_read_sensor()");
    // datasheet: http://www.aosong.com/userfiles/files/media/Data%20Sheet%20AHT20%20%20A2.pdf
    uint8_t data[6];
    uint8_t command[] = {0xAC, 0x33, 0x00}; //  0xAC command (trigger measurement). This command parameter has two bytes, the first byte is 0x33, and the second byte is 0x00
    ESP_ERROR_CHECK(i2c_master_transmit(aht30_handle, command, sizeof(command), I2C_MASTER_TIMEOUT));
    vTaskDelay(pdMS_TO_TICKS(80));
    ESP_ERROR_CHECK(i2c_master_receive(aht30_handle, data, sizeof(data), I2C_MASTER_TIMEOUT));
    uint8_t state = data[0];
    if ( state & 0x80 ) {
        ESP_LOGE(TAG, "%s", "AHT30 sensor data not ready");
        return ESP_ERR_NOT_FINISHED;
    } 
    uint32_t raw_humidity = (data[1] << 12) | (data[2] << 4) | (data[3] >> 4);
    uint32_t raw_temperature = ((data[3] & 0x0F) << 16) | (data[4] << 8) | data[5];
    *humidity = ((float) raw_humidity / 1048576) * 100;
    *temperature = ((float) raw_temperature / 1048576) * 200 - 50;
    return ESP_OK;
}
 */