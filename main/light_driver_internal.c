/*
 * SPDX-FileCopyrightText: 2024 Reasty
 *
 * SPDX-License-Identifier: CC0-1.0
 *
 * LitterBox.v1 - Simple GPIO LED driver for XIAO ESP32-C6
 *
 * GPIO15 onboard LED, Active-Low logic:
 *   power=true  -> gpio_set_level(0) -> LED ON
 *   power=false -> gpio_set_level(1) -> LED OFF
 */

#include "light_driver.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "LIGHT_DRIVER";

void light_driver_init(bool power)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LIGHT_LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    light_driver_set_power(power);
    ESP_LOGI(TAG, "LED driver initialized on GPIO%d (Active-Low), power: %s", LIGHT_LED_GPIO, power ? "ON" : "OFF");
}

void light_driver_set_power(bool power)
{
    /* Active-Low: power ON = level 0, power OFF = level 1 */
    gpio_set_level(LIGHT_LED_GPIO, power ? 0 : 1);
}
