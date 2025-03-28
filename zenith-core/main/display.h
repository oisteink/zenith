#pragma once

#include "esp_err.h"

esp_err_t display_init(void);
void display_update_values(float temperature, float humidity);