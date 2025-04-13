#include <stdio.h>
#include "zenith_sensor.h"
#include "esp_check.h"

static const char *TAG = "zenith_sensor";


// Move this to zenith_sensor_new_ath30()
/* esp_err_t zenith_sensor_create( zenith_sensor_config_t* config, zenith_sensor_handle_t* handle )
{
    // Sanity checks will come here
    
    zenith_sensor_handle_t sensor = calloc(1, sizeof(zenith_sensor_t));
    ESP_RETURN_ON_FALSE(
        sensor,
        ESP_ERR_NO_MEM,
        TAG, "Error creating sensor handle"
    );

    sensor->sensor_type = config->sensor_type;

    switch (sensor->sensor_type)
    {
        case SENSOR_ATH30:
            aht30_init(config->i2c_bus_handle);
            break;
        case SENSOR_BMP280:
            sensor->sensor_handle.bmp280 = bmp280_create(config->i2c_bus_handle, 0x78);
            break;
        default:
            ret = ESP_ERR_NOT_SUPPORTED; // Enum that lacks implementation?
            free( sensor );
            sensor = NULL;
            break; 
    }

    *handle = sensor;
    return ret;
}
 */