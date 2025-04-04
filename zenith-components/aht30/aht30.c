#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "aht30.h"

static const char *TAG = "AHT30";
static i2c_master_dev_handle_t aht30_handle;

/// @brief Init the i2c bus and add the i2c device
esp_err_t aht30_init(void)
{
    ESP_LOGI(TAG, "aht30_init()");
    i2c_master_bus_handle_t bus_handle; // Ingen behov utover init, kan gjenfinnes med i2c_master_get_bus_handle()
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_MASTER_NUM,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
    // Sensor-breakout har egen 10k pullup        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &bus_handle));

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
