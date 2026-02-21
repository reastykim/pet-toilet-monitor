/*
 * SPDX-FileCopyrightText: 2024 Reasty
 *
 * SPDX-License-Identifier: CC0-1.0
 *
 * LitterBox.v1 - Smart Pet Toilet Monitor
 * Based on ESP Zigbee HA_on_off_light example
 */

#pragma once

#include "esp_zigbee_core.h"
#include "light_driver.h"
#include "zcl_utility.h"
#include "air_sensor_driver.h"
#include "event_detector.h"

/* Zigbee configuration */
#define INSTALLCODE_POLICY_ENABLE       false   /* enable the install code policy for security */
#define ED_AGING_TIMEOUT                ESP_ZB_ED_AGING_TIMEOUT_64MIN
#define ED_KEEP_ALIVE                   3000    /* 3000 millisecond */
#define HA_LITTERBOX_ENDPOINT           1       /* LitterBox device endpoint (must be 1 for SmartThings) */
#define ESP_ZB_PRIMARY_CHANNEL_MASK     ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK  /* Zigbee primary channel mask */

/* Basic manufacturer information (ZCL string format: length byte + string) */
#define ESP_MANUFACTURER_NAME "\x06""Reasty"         /* 6 bytes: "Reasty" */
#define ESP_MODEL_IDENTIFIER  "\x0c""LitterBox.v1"   /* 12 bytes: "LitterBox.v1" */

/* Custom NH₃ Concentration Measurement Cluster (Manufacturer-Specific, 0xFC00)
 * ZCL has no standard cluster for NH₃. Using manufacturer-specific cluster
 * with uint16 ppm directly (simpler than CO₂ cluster float fraction approach).
 */
#define NH3_CUSTOM_CLUSTER_ID           0xFC00  /* Manufacturer-specific cluster */
#define NH3_ATTR_MEASURED_VALUE_ID      0x0000  /* Measured value: uint16, ppm */
#define NH3_ATTR_MIN_MEASURED_VALUE_ID  0x0001  /* Min measurable: uint16, ppm */
#define NH3_ATTR_MAX_MEASURED_VALUE_ID  0x0002  /* Max measurable: uint16, ppm */
#define NH3_ATTR_EVENT_TYPE_ID          0x0003  /* Event type: uint8 (0=none, 1=urination, 2=defecation) */
#define NH3_DEFAULT_PPM                 0       /* Fallback when sensor read fails */
#define NH3_MIN_PPM                     0
#define NH3_MAX_PPM                     1000

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
