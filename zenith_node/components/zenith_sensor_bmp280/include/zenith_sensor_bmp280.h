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

// 3.3.1 Temperature: Table 21
// 3.3.2 Pressure : Table 22
typedef enum bmp280_oversampling_e {
    BMP280_OVERSAMPLING_NONE = 0b000,
    BMP280_OVERSAMPLING_X1   = 0b001,
    BMP280_OVERSAMPLING_X2   = 0b010,
    BMP280_OVERSAMPLING_X4   = 0b011,
    BMP280_OVERSAMPLING_X8   = 0b100,
    BMP280_OVERSAMPLING_X16  = 0b101
} bmp280_oversampling_t;

// 3.6 Power Modes: Table 10
typedef enum bmp280_power_mode_e {
    BMP280_MODE_SLEEP  = 0b00,
    BMP280_MODE_FORCED = 0b01,
    BMP280_MODE_NORMAL = 0b11
} bmp280_power_mode_t;

// 3.6.3 Inactive duration in normal mode: Table 11
typedef enum bmp280_standby_e {
    BMP280_STANDBY_MS_0_5  = 0b000,
    BMP280_STANDBY_MS_10   = 0b110,
    BMP280_STANDBY_MS_20   = 0b111,
    BMP280_STANDBY_MS_62_5 = 0b001,
    BMP280_STANDBY_MS_125  = 0b010,
    BMP280_STANDBY_MS_250  = 0b011,
    BMP280_STANDBY_MS_500  = 0b100,
    BMP280_STANDBY_MS_1000 = 0b101
} bmp280_standby_t;

// 3.3.3 IIR filter: Table 6
typedef enum bmp280_iir_filters_e {
    BMP280_FILTER_OFF = 0b000,
    BMP280_FILTER_X2  = 0b001,
    BMP280_FILTER_X4  = 0b010,
    BMP280_FILTER_X8  = 0b011,
    BMP280_FILTER_X16 = 0b100
} bmp280_IIR_filter_t;

typedef struct bmp280_config_s {
    unsigned int t_sb : 3;
    unsigned int filter : 3;
    unsigned int unused : 1;
    unsigned int spi3w_en : 1;
} bmp280_config_t;

typedef struct ctrl_meas_s {
    unsigned int osrs_t : 3;
    unsigned int osrs_p : 3;
    unsigned int mode : 2;
} bmp280_ctrl_meas_t;

typedef struct status_s {
    unsigned int unused : 4;
    unsigned int measuring : 3;
    unsigned int unused2 : 1;
    unsigned int im_update : 1;
} status_t;

typedef struct zenith_sensor_bmp280_config_s {
    uint16_t device_address; // 
    uint32_t scl_speed_hz;   // 100kHz is max i2c standard mode
    bmp280_config_t config;
    bmp280_ctrl_meas_t control_measure;
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
