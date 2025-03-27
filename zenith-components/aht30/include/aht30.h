#pragma once

#include "esp_err.h"
#include "driver/i2c_master.h"

#define I2C_MASTER_SCL_IO    20
#define I2C_MASTER_SDA_IO    19
#define I2C_MASTER_NUM       I2C_NUM_0
#define I2C_MASTER_FREQ_HZ   100000
#define I2C_MASTER_TIMEOUT   1000

#define AHT30_SENSOR_ADDR  0x38

void aht30_init(void);

esp_err_t aht30_read_sensor(float *temperature, float *humidity);