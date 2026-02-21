/*
 * SPDX-FileCopyrightText: 2024 Reasty
 *
 * SPDX-License-Identifier: CC0-1.0
 *
 * LitterBox.v1 - Smart Cat Litter Box Monitoring System
 *
 * Based on ESP Zigbee HA_temperature_sensor example.
 * Clusters: Custom NH₃ Concentration (0xFC00) + On/Off (0x0006)
 * Custom manufacturer-specific cluster reports NH₃ ppm as uint16 directly.
 */
#include "main.h"
#include "esp_check.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ha/esp_zigbee_ha_standard.h"

#if !defined ZB_ED_ROLE
#error Define ZB_ED_ROLE in idf.py menuconfig to compile light (End Device) source code.
#endif

static const char *TAG = "LITTERBOX";

/* Event detection state — persists across timer callbacks */
static event_detector_t g_detector;
static litter_event_t   g_last_reported_event = LITTER_EVENT_NONE;

/********************* Deferred driver init **********************/

static esp_err_t deferred_driver_init(void)
{
    static bool is_inited = false;
    if (!is_inited) {
        light_driver_init(LIGHT_DEFAULT_OFF);
        esp_err_t ret = air_sensor_init();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Air sensor init failed (%s) — will use fallback value", esp_err_to_name(ret));
        }
        event_detector_init(&g_detector);
        is_inited = true;
    }
    return is_inited ? ESP_OK : ESP_FAIL;
}

/********************* Sensor report timer ***********************/

static void sensor_report_timer_cb(uint8_t param)
{
    /* Read NH₃ concentration from MQ-135 sensor */
    air_sensor_data_t sensor = {0};
    uint16_t nh3_ppm;

    float ppm_f = 0.0f;   /* float ppm for event detector (higher precision) */

    if (air_sensor_read(&sensor) == ESP_OK && sensor.is_valid) {
        nh3_ppm = sensor.nh3_ppm;
        ppm_f   = sensor.nh3_ppm_f;
        if (sensor.is_warming_up) {
            ESP_LOGI(TAG, "Sensor warming up (raw=%"PRIu32"), NH3=%.1f ppm (unreliable)",
                     sensor.raw_adc, (double)ppm_f);
        }
    } else {
        nh3_ppm = NH3_DEFAULT_PPM;
        ppm_f   = 0.0f;
        ESP_LOGW(TAG, "Sensor read failed — reporting fallback: %u ppm", nh3_ppm);
    }

    /* Run event detection state machine with float ppm (outside lock — pure computation) */
    litter_event_t new_event = event_detector_update(&g_detector, ppm_f);
    bool event_changed = (new_event != g_last_reported_event);

    esp_zb_lock_acquire(portMAX_DELAY);

    /* --- NH₃ ppm Report (custom cluster 0xFC00, attr 0x0000) --- */
    esp_zb_zcl_set_attribute_val(
        HA_LITTERBOX_ENDPOINT,
        NH3_CUSTOM_CLUSTER_ID,
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        NH3_ATTR_MEASURED_VALUE_ID,
        &nh3_ppm, false);

    esp_zb_zcl_report_attr_cmd_t nh3_report = {0};
    nh3_report.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
    nh3_report.attributeID = NH3_ATTR_MEASURED_VALUE_ID;
    nh3_report.direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI;
    nh3_report.clusterID = NH3_CUSTOM_CLUSTER_ID;
    nh3_report.zcl_basic_cmd.src_endpoint = HA_LITTERBOX_ENDPOINT;
    nh3_report.zcl_basic_cmd.dst_addr_u.addr_short = 0x0000;
    nh3_report.zcl_basic_cmd.dst_endpoint = 1;
    esp_zb_zcl_report_attr_cmd_req(&nh3_report);

    /* --- Event Type Report (attr 0x0003) — only when event changes --- */
    if (event_changed) {
        uint8_t event_val = (uint8_t)new_event;
        esp_zb_zcl_set_attribute_val(
            HA_LITTERBOX_ENDPOINT,
            NH3_CUSTOM_CLUSTER_ID,
            ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
            NH3_ATTR_EVENT_TYPE_ID,
            &event_val, false);

        esp_zb_zcl_report_attr_cmd_t event_report = {0};
        event_report.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
        event_report.attributeID = NH3_ATTR_EVENT_TYPE_ID;
        event_report.direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI;
        event_report.clusterID = NH3_CUSTOM_CLUSTER_ID;
        event_report.zcl_basic_cmd.src_endpoint = HA_LITTERBOX_ENDPOINT;
        event_report.zcl_basic_cmd.dst_addr_u.addr_short = 0x0000;
        event_report.zcl_basic_cmd.dst_endpoint = 1;
        esp_zb_zcl_report_attr_cmd_req(&event_report);
    }

    esp_zb_lock_release();

    ESP_LOGI(TAG, "Reported NH3=%u ppm (%.1f ppm_f, baseline=%.1f, raw=%"PRIu32")",
             nh3_ppm, (double)ppm_f, event_detector_get_baseline(&g_detector), sensor.raw_adc);

    if (event_changed) {
        g_last_reported_event = new_event;
        static const char *event_names[] = {"NONE", "URINATION", "DEFECATION"};
        ESP_LOGI(TAG, "Event type changed → %s (%u)", event_names[new_event], (unsigned)new_event);
    }

    /* Re-schedule */
    esp_zb_scheduler_alarm((esp_zb_callback_t)sensor_report_timer_cb, 0, SENSOR_REPORT_INTERVAL_MS);
}

/********************* Zigbee signal handler **********************/

static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask)
{
    ESP_RETURN_ON_FALSE(esp_zb_bdb_start_top_level_commissioning(mode_mask) == ESP_OK, , TAG, "Failed to start Zigbee commissioning");
}

void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    uint32_t *p_sg_p       = signal_struct->p_app_signal;
    esp_err_t err_status = signal_struct->esp_err_status;
    esp_zb_app_signal_type_t sig_type = *p_sg_p;
    switch (sig_type) {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
        ESP_LOGI(TAG, "Initialize Zigbee stack");
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
        break;
    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
        if (err_status == ESP_OK) {
            ESP_LOGI(TAG, "Deferred driver initialization %s", deferred_driver_init() ? "failed" : "successful");
            ESP_LOGI(TAG, "Device started up in%s factory-reset mode", esp_zb_bdb_is_factory_new() ? "" : " non");
            if (esp_zb_bdb_is_factory_new()) {
                ESP_LOGI(TAG, "Start network steering");
                esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
            } else {
                ESP_LOGI(TAG, "Device rebooted, already on network - starting reports");
                esp_zb_scheduler_alarm((esp_zb_callback_t)sensor_report_timer_cb, 0, SENSOR_REPORT_INTERVAL_MS);
            }
        } else {
            ESP_LOGW(TAG, "%s failed with status: %s, retrying", esp_zb_zdo_signal_to_string(sig_type),
                     esp_err_to_name(err_status));
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb,
                                   ESP_ZB_BDB_MODE_INITIALIZATION, 1000);
        }
        break;
    case ESP_ZB_BDB_SIGNAL_STEERING:
        if (err_status == ESP_OK) {
            esp_zb_ieee_addr_t extended_pan_id;
            esp_zb_get_extended_pan_id(extended_pan_id);
            ESP_LOGI(TAG, "Joined network successfully (Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: 0x%04hx, Channel:%d, Short Address: 0x%04hx)",
                     extended_pan_id[7], extended_pan_id[6], extended_pan_id[5], extended_pan_id[4],
                     extended_pan_id[3], extended_pan_id[2], extended_pan_id[1], extended_pan_id[0],
                     esp_zb_get_pan_id(), esp_zb_get_current_channel(), esp_zb_get_short_address());
            /* Start sensor reporting NOW (after joining network) */
            esp_zb_scheduler_alarm((esp_zb_callback_t)sensor_report_timer_cb, 0, SENSOR_REPORT_INTERVAL_MS);
            ESP_LOGI(TAG, "Sensor report timer started (interval: %d ms)", SENSOR_REPORT_INTERVAL_MS);
        } else {
            ESP_LOGI(TAG, "Network steering was not successful (status: %s)", esp_err_to_name(err_status));
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
        }
        break;
    default:
        ESP_LOGI(TAG, "ZDO signal: %s (0x%x), status: %s", esp_zb_zdo_signal_to_string(sig_type), sig_type,
                 esp_err_to_name(err_status));
        break;
    }
}

/********************* Attribute & Action handlers *************************/

static esp_err_t zb_attribute_handler(const esp_zb_zcl_set_attr_value_message_t *message)
{
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(message, ESP_FAIL, TAG, "Empty message");
    ESP_RETURN_ON_FALSE(message->info.status == ESP_ZB_ZCL_STATUS_SUCCESS, ESP_ERR_INVALID_ARG, TAG, "Received message: error status(%d)",
                        message->info.status);
    ESP_LOGI(TAG, "Set attribute: endpoint(%d), cluster(0x%x), attribute(0x%x), data size(%d)",
             message->info.dst_endpoint, message->info.cluster,
             message->attribute.id, message->attribute.data.size);
    if (message->info.dst_endpoint == HA_LITTERBOX_ENDPOINT) {
        if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF) {
            if (message->attribute.id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_BOOL) {
                bool light_state = message->attribute.data.value ? *(bool *)message->attribute.data.value : false;
                ESP_LOGI(TAG, "Light sets to %s", light_state ? "On" : "Off");
                light_driver_set_power(light_state);
            }
        }
    }
    return ret;
}

static esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message)
{
    esp_err_t ret = ESP_OK;
    switch (callback_id) {
    case ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID:
        ret = zb_attribute_handler((esp_zb_zcl_set_attr_value_message_t *)message);
        break;
    case ESP_ZB_CORE_CMD_DEFAULT_RESP_CB_ID: {
        const esp_zb_zcl_cmd_default_resp_message_t *resp = (const esp_zb_zcl_cmd_default_resp_message_t *)message;
        if (resp) {
            ESP_LOGI(TAG, "Default Response: status=0x%02x, cmd_id=0x%02x, cluster=0x%04x, src_ep=%d, dst_ep=%d",
                     resp->status_code, resp->resp_to_cmd,
                     resp->info.cluster, resp->info.src_endpoint, resp->info.dst_endpoint);
        }
        break;
    }
    default:
        ESP_LOGW(TAG, "Receive Zigbee action(0x%x) callback", callback_id);
        break;
    }
    return ret;
}

/********************* Cluster creation **************************/

static esp_zb_cluster_list_t *custom_litterbox_clusters_create(void)
{
    esp_zb_cluster_list_t *cluster_list = esp_zb_zcl_cluster_list_create();

    /* Basic Cluster with manufacturer info */
    esp_zb_basic_cluster_cfg_t basic_cfg = { .zcl_version = ESP_ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE, .power_source = 0x04 /* DC */ };
    esp_zb_attribute_list_t *basic_cluster = esp_zb_basic_cluster_create(&basic_cfg);
    ESP_ERROR_CHECK(esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, ESP_MANUFACTURER_NAME));
    ESP_ERROR_CHECK(esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, ESP_MODEL_IDENTIFIER));
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_basic_cluster(cluster_list, basic_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));

    /* Identify Cluster - server + client roles */
    esp_zb_identify_cluster_cfg_t identify_cfg = { .identify_time = ESP_ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE };
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_identify_cluster(cluster_list, esp_zb_identify_cluster_create(&identify_cfg), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_identify_cluster(cluster_list, esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY), ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE));

    /* Custom NH₃ Concentration Measurement Cluster (0xFC00, Manufacturer-Specific)
     * Uses uint16 ppm directly instead of CO₂ cluster's float fraction (0.0~1.0).
     * Attributes mirror ZCL Concentration Measurement structure (0x040C~0x042B). */
    uint16_t nh3_measured = NH3_DEFAULT_PPM;
    uint16_t nh3_min = NH3_MIN_PPM;
    uint16_t nh3_max = NH3_MAX_PPM;
    uint8_t  nh3_event_type = (uint8_t)LITTER_EVENT_NONE;
    esp_zb_attribute_list_t *nh3_cluster = esp_zb_zcl_attr_list_create(NH3_CUSTOM_CLUSTER_ID);
    ESP_ERROR_CHECK(esp_zb_custom_cluster_add_custom_attr(nh3_cluster,
        NH3_ATTR_MEASURED_VALUE_ID, ESP_ZB_ZCL_ATTR_TYPE_U16,
        ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY | ESP_ZB_ZCL_ATTR_ACCESS_REPORTING,
        &nh3_measured));
    ESP_ERROR_CHECK(esp_zb_custom_cluster_add_custom_attr(nh3_cluster,
        NH3_ATTR_MIN_MEASURED_VALUE_ID, ESP_ZB_ZCL_ATTR_TYPE_U16,
        ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY,
        &nh3_min));
    ESP_ERROR_CHECK(esp_zb_custom_cluster_add_custom_attr(nh3_cluster,
        NH3_ATTR_MAX_MEASURED_VALUE_ID, ESP_ZB_ZCL_ATTR_TYPE_U16,
        ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY,
        &nh3_max));
    ESP_ERROR_CHECK(esp_zb_custom_cluster_add_custom_attr(nh3_cluster,
        NH3_ATTR_EVENT_TYPE_ID, ESP_ZB_ZCL_ATTR_TYPE_U8,
        ESP_ZB_ZCL_ATTR_ACCESS_READ_ONLY | ESP_ZB_ZCL_ATTR_ACCESS_REPORTING,
        &nh3_event_type));
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_custom_cluster(cluster_list, nh3_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));

    /* On/Off Cluster (for LED control) */
    esp_zb_on_off_cluster_cfg_t on_off_cfg = { .on_off = false };
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_on_off_cluster(cluster_list, esp_zb_on_off_cluster_create(&on_off_cfg), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));

    return cluster_list;
}

static esp_zb_ep_list_t *custom_litterbox_ep_create(uint8_t endpoint_id)
{
    esp_zb_ep_list_t *ep_list = esp_zb_ep_list_create();
    esp_zb_endpoint_config_t endpoint_config = {
        .endpoint = endpoint_id,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_CUSTOM_ATTR_DEVICE_ID,
        .app_device_version = 0
    };
    esp_zb_ep_list_add_ep(ep_list, custom_litterbox_clusters_create(), endpoint_config);
    return ep_list;
}

/********************* Main Zigbee task **************************/

static void esp_zb_task(void *pvParameters)
{
    /* Initialize Zigbee stack */
    esp_zb_cfg_t zb_nwk_cfg = ESP_ZB_ZED_CONFIG();
    esp_zb_init(&zb_nwk_cfg);

    /* Create customized LitterBox endpoint */
    esp_zb_ep_list_t *esp_zb_litterbox_ep = custom_litterbox_ep_create(HA_LITTERBOX_ENDPOINT);

    /* Register the device */
    esp_zb_device_register(esp_zb_litterbox_ep);

    /* Note: esp_zb_zcl_update_reporting_info() is NOT used for the custom NH₃ cluster
     * (0xFC00) because the ZCL stack's internal reporting mechanism does not support
     * manufacturer-specific clusters and will crash. Instead, reports are sent manually
     * via esp_zb_zcl_report_attr_cmd_req() in sensor_report_timer_cb(). */

    /* Register action handler */
    esp_zb_core_action_handler_register(zb_action_handler);

    esp_zb_set_primary_network_channel_set(ESP_ZB_PRIMARY_CHANNEL_MASK);
    ESP_ERROR_CHECK(esp_zb_start(false));
    esp_zb_stack_main_loop();
}

/********************* App main **********************************/

void app_main(void)
{
    esp_zb_platform_config_t config = {
        .radio_config = ESP_ZB_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_ZB_DEFAULT_HOST_CONFIG(),
    };
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_zb_platform_config(&config));
    xTaskCreate(esp_zb_task, "Zigbee_main", 4096, NULL, 5, NULL);
}
