/*
 * SPDX-FileCopyrightText: 2024 Reasty
 *
 * SPDX-License-Identifier: CC0-1.0
 *
 * LitterBox.v1 - Air Quality Sensor Driver Interface
 *
 * Abstract interface for NH₃ concentration sensing.
 * Concrete implementation: air_sensor_driver_MQ135.c
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* MQ-135 heater warmup time before readings are reliable */
#define AIR_SENSOR_WARMUP_MS    20000

/**
 * @brief Sensor reading result.
 *
 * Always check `is_valid` before using nh3_ppm.
 * During warmup `is_warming_up` is true — values are recorded but unreliable.
 */
typedef struct {
    uint16_t nh3_ppm;       /**< NH₃ concentration (ppm), 0–1000 */
    uint32_t raw_adc;       /**< Raw 12-bit ADC value (0–4095), for diagnostics */
    bool     is_warming_up; /**< True while sensor heater is warming up */
    bool     is_valid;      /**< False on ADC read error; true otherwise */
} air_sensor_data_t;

/**
 * @brief Initialize the air quality sensor.
 *        Must be called once before air_sensor_read().
 *        Starts the internal warmup timer.
 *
 * @return ESP_OK on success.
 */
esp_err_t air_sensor_init(void);

/**
 * @brief Read the current NH₃ concentration.
 *
 * @param[out] out  Populated with sensor data. Never NULL.
 * @return ESP_OK on success, ESP_ERR_INVALID_STATE if not initialized.
 */
esp_err_t air_sensor_read(air_sensor_data_t *out);

#ifdef __cplusplus
}
#endif
