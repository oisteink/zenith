#pragma once

#include "esp_err.h"
#include "driver/i2c_master.h"
#include "zenith_sensor.h"

/////////////////////////////////
// Sensor can use SPI3/4 wire or i2c
// 
// Interface selection
// Interface selection is done automatically based on CSB (chip select) status. If CSB is connected to VDDio, the i2c interface is active. If CSB is pulled down, the SPI interface is activated.
// 
// i2c interface
// Adressing: The 6 MSB are fixed to 111011x - last bit is changable by SDO value and can be changed during operation
// Ground = 0 (0x76), VDDio = 1 (0x77). Leaving SDO floating gives undefined i2c address
//
// Recomended filter settings for Weather Monitoring is:
// - Mode Normal
// - Oversampling: Ultra low power
// - osrs_p: x1
// - osrs_t: x1
// - IIR filter coeff.: OFF
// - Idd (uA): 0.14
// - ODF (Hz): 1/60
// - RMS Noise (cm): 26.4
// 
// Temp is      0xF7 << 12 | 0xF8 << 4 | 0xF9 >> 4
// Pressure is  0xFA << 12 | 0xFB << 4 | 0xFC >> 4

#define DEFAULT_ZENITH_SENSOR_BMP280_CONFIG { .device_address = 0x76, .scl_speed_hz = 100 * 1000 }

typedef struct zenith_sensor_bmp280_config_s {
    uint16_t device_address; // 
    uint32_t scl_speed_hz;   // 100kHz is max i2c standard mode
} zenith_sensor_bmp280_config_t;


typedef struct zenith_sensor_bmp280_s zenith_sensor_bmp280_t;
typedef zenith_sensor_bmp280_t *zenith_sensor_bmp280_handle_t;

struct zenith_sensor_bmp280_s {
    zenith_sensor_t base;
    i2c_master_bus_handle_t bus_handle;
    i2c_master_dev_handle_t dev_handle;
    zenith_sensor_bmp280_config_t config;
}; 

esp_err_t zenith_sensor_new_bmp280( i2c_master_bus_handle_t i2c_bus, zenith_sensor_bmp280_config_t *config, zenith_sensor_handle_t *handle);
