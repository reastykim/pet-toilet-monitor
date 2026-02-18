/*
 * SPDX-FileCopyrightText: 2024 Reasty
 *
 * SPDX-License-Identifier: CC0-1.0
 *
 * LitterBox.v1 - Smart Cat Litter Box Monitoring System
 * Based on ESP Zigbee HA_on_off_light example
 */

#pragma once

#include "esp_zigbee_core.h"
#include "light_driver.h"
#include "zcl_utility.h"

/* Zigbee configuration */
#define INSTALLCODE_POLICY_ENABLE       false   /* enable the install code policy for security */
#define ED_AGING_TIMEOUT                ESP_ZB_ED_AGING_TIMEOUT_64MIN
#define ED_KEEP_ALIVE                   3000    /* 3000 millisecond */
#define HA_LITTERBOX_ENDPOINT           1       /* LitterBox device endpoint (must be 1 for SmartThings) */
#define ESP_ZB_PRIMARY_CHANNEL_MASK     ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK  /* Zigbee primary channel mask */

/* Basic manufacturer information (ZCL string format: length byte + string) */
#define ESP_MANUFACTURER_NAME "\x06""Reasty"         /* 6 bytes: "Reasty" */
#define ESP_MODEL_IDENTIFIER  "\x0c""LitterBox.v1"   /* 12 bytes: "LitterBox.v1" */

/* CO₂ / NH₃ sensor configuration (using CO₂ cluster 0x040D to map NH₃ ppm) */
#define CO2_SENSOR_MIN_MEASURED_VALUE   0.0f    /* Min: 0 ppm as ZCL fraction */
#define CO2_SENSOR_MAX_MEASURED_VALUE   0.001f  /* Max: 1000 ppm as ZCL fraction */

/* Sensor report interval */
#define SENSOR_REPORT_INTERVAL_MS       10000   /* Report interval: 10 seconds */

#define ESP_ZB_ZED_CONFIG()                                         \
    {                                                               \
        .esp_zb_role = ESP_ZB_DEVICE_TYPE_ED,                       \
        .install_code_policy = INSTALLCODE_POLICY_ENABLE,           \
        .nwk_cfg.zed_cfg = {                                        \
            .ed_timeout = ED_AGING_TIMEOUT,                         \
            .keep_alive = ED_KEEP_ALIVE,                            \
        },                                                          \
    }

#define ESP_ZB_DEFAULT_RADIO_CONFIG()                           \
    {                                                           \
        .radio_mode = ZB_RADIO_MODE_NATIVE,                     \
    }

#define ESP_ZB_DEFAULT_HOST_CONFIG()                            \
    {                                                           \
        .host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE,   \
    }
