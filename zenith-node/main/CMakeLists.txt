idf_component_register(SRCS "zenith-node.c"
                    INCLUDE_DIRS "."
                    REQUIRES "zenith_sensor_aht30" "zenith_sensor" "zenith-now" "zenith-blink" "esp_wifi" "nvs_flash") 
