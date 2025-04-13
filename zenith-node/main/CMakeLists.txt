idf_component_register(SRCS "zenith-node.c"
                    INCLUDE_DIRS "."
                    REQUIRES "aht30" "zenith_sensor" "zenith-now" "zenith-blink" "esp_wifi" "nvs_flash") 
