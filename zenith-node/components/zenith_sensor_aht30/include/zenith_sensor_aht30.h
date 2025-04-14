#pragma once

#include "esp_err.h"
#include "driver/i2c_master.h"
#include "zenith_sensor.h"

#define AHT30_SENSOR_TIMEOUT   1000  // could end up in config, but most users don't need to think about this.

#define DEFAULT_ZENITH_SENSOR_AHT30_CONFIG { .device_address = 0x38, .scl_speed_hz = 100 * 1000 }

typedef struct zenith_sensor_aht30_config_s {
    uint16_t device_address; // Not really needed as the address is static. 
    uint32_t scl_speed_hz;   // 100kHz is max i2c standard mode
} zenith_sensor_aht30_config_t;


typedef struct zenith_sensor_aht30_s zenith_sensor_aht30_t;
typedef zenith_sensor_aht30_t *zenith_sensor_aht30_handle_t;

struct zenith_sensor_aht30_s {
    zenith_sensor_t base;
    i2c_master_bus_handle_t bus_handle;
    i2c_master_dev_handle_t dev_handle;
    zenith_sensor_aht30_config_t config;
}; 

esp_err_t zenith_sensor_new_aht30( i2c_master_bus_handle_t i2c_bus, zenith_sensor_aht30_config_t *config, zenith_sensor_handle_t *handle);