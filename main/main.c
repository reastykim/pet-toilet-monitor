/*
 * SPDX-FileCopyrightText: 2024 Reasty
 *
 * SPDX-License-Identifier: CC0-1.0
 *
 * LitterBox.v1 - Smart Cat Litter Box Monitoring System
 *
 * Based on ESP Zigbee HA_temperature_sensor example.
 * Clusters: CO₂ Concentration (0x040D) + On/Off (0x0006)
 * CO₂ cluster is used to map NH₃ (ammonia) ppm from MQ-135 sensor.
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

/********************* Deferred driver init **********************/

static esp_err_t deferred_driver_init(void)
{
    static bool is_inited = false;
    if (!is_inited) {
        light_driver_init(LIGHT_DEFAULT_OFF);
        is_inited = true;
    }
    return is_inited ? ESP_OK : ESP_FAIL;
}

/********************* Sensor report timer ***********************/

static void sensor_report_timer_cb(uint8_t param)
{
    /* Dummy value - to be replaced by real MQ-135 sensor reading */
    float co2_value = 0.00005f;   /* 50 ppm as ZCL fraction (ppm / 1,000,000) */

    esp_zb_lock_acquire(portMAX_DELAY);

    /* --- CO₂ / NH₃ Report --- */
    esp_zb_zcl_set_attribute_val(
        HA_LITTERBOX_ENDPOINT,
        ESP_ZB_ZCL_CLUSTER_ID_CARBON_DIOXIDE_MEASUREMENT,
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        ESP_ZB_ZCL_ATTR_CARBON_DIOXIDE_MEASUREMENT_MEASURED_VALUE_ID,
        &co2_value, false);

    esp_zb_zcl_report_attr_cmd_t co2_report = {0};
    co2_report.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
    co2_report.attributeID = ESP_ZB_ZCL_ATTR_CARBON_DIOXIDE_MEASUREMENT_MEASURED_VALUE_ID;
    co2_report.direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI;
    co2_report.clusterID = ESP_ZB_ZCL_CLUSTER_ID_CARBON_DIOXIDE_MEASUREMENT;
    co2_report.zcl_basic_cmd.src_endpoint = HA_LITTERBOX_ENDPOINT;
    co2_report.zcl_basic_cmd.dst_addr_u.addr_short = 0x0000;
    co2_report.zcl_basic_cmd.dst_endpoint = 1;
    esp_zb_zcl_report_attr_cmd_req(&co2_report);

    esp_zb_lock_release();

    ESP_LOGI(TAG, "Reported NH3=%.0f ppm", co2_value * 1000000.0f);

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

    /* CO₂ Concentration Measurement Cluster (used to map NH₃ ppm from MQ-135) */
    esp_zb_carbon_dioxide_measurement_cluster_cfg_t co2_cfg = {
        .measured_value = 0.0f,
        .min_measured_value = CO2_SENSOR_MIN_MEASURED_VALUE,
        .max_measured_value = CO2_SENSOR_MAX_MEASURED_VALUE,
    };
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_carbon_dioxide_measurement_cluster(cluster_list,
        esp_zb_carbon_dioxide_measurement_cluster_create(&co2_cfg), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));

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

    /* Config the reporting info for CO₂ / NH₃ */
    esp_zb_zcl_reporting_info_t co2_reporting_info = {
        .direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV,
        .ep = HA_LITTERBOX_ENDPOINT,
        .cluster_id = ESP_ZB_ZCL_CLUSTER_ID_CARBON_DIOXIDE_MEASUREMENT,
        .cluster_role = ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        .dst.profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .u.send_info.min_interval = 1,
        .u.send_info.max_interval = 0,
        .u.send_info.def_min_interval = 1,
        .u.send_info.def_max_interval = 0,
        .attr_id = ESP_ZB_ZCL_ATTR_CARBON_DIOXIDE_MEASUREMENT_MEASURED_VALUE_ID,
        .manuf_code = ESP_ZB_ZCL_ATTR_NON_MANUFACTURER_SPECIFIC,
    };
    esp_zb_zcl_update_reporting_info(&co2_reporting_info);

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
