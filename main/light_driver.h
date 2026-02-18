/*
 * SPDX-FileCopyrightText: 2024 Reasty
 *
 * SPDX-License-Identifier: CC0-1.0
 *
 * LitterBox.v1 - Simple GPIO LED driver for XIAO ESP32-C6
 *
 * This example code is in the Public Domain (or CC0 licensed, at your option.)
 */

#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Light intensity level */
#define LIGHT_DEFAULT_ON  1
#define LIGHT_DEFAULT_OFF 0

/* XIAO ESP32-C6 onboard LED configuration */
#define LIGHT_LED_GPIO    15    /* GPIO15: onboard LED on XIAO ESP32-C6 */
#define LIGHT_ACTIVE_LOW  1     /* Active-Low: LOW = LED ON, HIGH = LED OFF */

/**
 * @brief Initialize the LED driver
 *
 * @param power Initial power state (LIGHT_DEFAULT_ON or LIGHT_DEFAULT_OFF)
 */
void light_driver_init(bool power);

/**
 * @brief Set LED power (on/off)
 *
 * @param power true = LED on, false = LED off
 */
void light_driver_set_power(bool power);

#ifdef __cplusplus
} // extern "C"
#endif
