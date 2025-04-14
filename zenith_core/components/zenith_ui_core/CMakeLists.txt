idf_component_register(SRCS "lvgl_ui.c" "zenith_ui_core.c"
                    INCLUDE_DIRS "include"
                    REQUIRES "esp_lcd_ili9488" "zenith_registry")
