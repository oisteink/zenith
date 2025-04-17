idf_component_register(SRCS "zenith_node.c"
                    INCLUDE_DIRS "."
                    REQUIRES "zenith_datapoints" "zenith_sensor_bmp280" "zenith_sensor_aht30" "zenith_sensor" "zenith_now" "zenith_blink" "esp_wifi" "nvs_flash") 
