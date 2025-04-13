#pragma once

#include "esp_err.h"
#include "driver/i2c_master.h"

#define I2C_MASTER_SCL_IO    20
#define I2C_MASTER_SDA_IO    19
#define I2C_MASTER_NUM       I2C_NUM_0

#define AHT30_SENSOR_FREQ_HZ   100000
#define AHT30_SENSOR_TIMEOUT   1000

#define AHT30_SENSOR_ADDR  0x38

///////////////
// Don't think I need a config struct for this - the only parameter is the address, and that's static afaik.

typedef struct aht30_sensor_s aht30_sensor_t;
typedef aht30_sensor_t *aht30_sensor_handle_t;

struct aht30_sensor_s {
    i2c_master_bus_handle_t i2c_bus_handle;
    i2c_bus_device_handle_t i2c_device_handle;
}; 

esp_err_t aht30_init(i2c_master_bus_handle_t bus_handle, aht30_sensor_handle_t * handle);

esp_err_t aht30_read_sensor(float *temperature, float *humidity);