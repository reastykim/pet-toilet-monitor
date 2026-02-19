/*
 * SPDX-FileCopyrightText: 2024 Reasty
 *
 * SPDX-License-Identifier: CC0-1.0
 *
 * LitterBox.v1 - MQ-135 Air Quality Sensor Driver
 *
 * Reads NH₃ (ammonia) concentration via ESP32-C6 ADC and converts the
 * raw voltage to ppm using the MQ-135 sensitivity curve.
 *
 * ── Hardware wiring ──────────────────────────────────────────────────────
 *  MQ-135 AOUT → GPIO2 (XIAO ESP32-C6 A0 = ADC1_CHANNEL_2)
 *
 *  ⚠ VOLTAGE WARNING:
 *    MQ-135 heater requires 5 V (VH pin). AOUT can swing up to VCC.
 *    ESP32-C6 ADC maximum input is 3.3 V.
 *    Options to stay safe:
 *      A) Power the MQ-135 module at 3.3 V (reduced sensitivity, but safe).
 *      B) Add a voltage divider on AOUT: e.g. 100 kΩ + 100 kΩ → halves voltage.
 *    This driver assumes option A (VCC = 3.3 V).
 *
 * ── Electrical model ─────────────────────────────────────────────────────
 *  AOUT  = VCC × RL / (Rs + RL)
 *  Rs    = RL × (VCC − AOUT) / AOUT       [sensor resistance]
 *  Rs/R0 → NH₃ ppm via sensitivity curve  [R0 = resistance in clean air]
 *
 * ── Calibration ──────────────────────────────────────────────────────────
 *  R0 (MQ135_R0_KOHM) must be measured per sensor unit:
 *    1. Power on in clean outdoor air.
 *    2. Wait ≥ 20 minutes for full thermal stabilization.
 *    3. Record AOUT voltage (Vmeas).
 *    4. R0 = RL × (VCC − Vmeas) / Vmeas
 *    5. Update MQ135_R0_KOHM and rebuild.
 *
 * ── NH₃ sensitivity curve ────────────────────────────────────────────────
 *  ppm = A × (Rs/R0)^B
 *  A = 102.2, B = −2.473   (empirical from MQ-135 datasheet + library)
 */

#include "air_sensor_driver.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_check.h"
#include "esp_adc/adc_oneshot.h"
#include <math.h>

static const char *TAG = "MQ135";

/* ── ADC configuration ──────────────────────────────────────────────── */
#define MQ135_ADC_UNIT          ADC_UNIT_1
#define MQ135_ADC_CHANNEL       ADC_CHANNEL_2   /* GPIO2 = XIAO A0 */
#define MQ135_ADC_ATTEN         ADC_ATTEN_DB_12 /* Input range 0 ~ 3.1 V */
#define MQ135_ADC_BITWIDTH      ADC_BITWIDTH_12  /* 0 ~ 4095 */

/* ── Electrical constants ───────────────────────────────────────────── */
#define MQ135_LOAD_RESISTANCE_KOHM  10.0f   /* RL on module board (kΩ) */
#define MQ135_VCC                   3.3f    /* Sensor supply voltage (V) */

/* ── NH₃ sensitivity curve ──────────────────────────────────────────── */
#define MQ135_NH3_CURVE_A   102.2f
#define MQ135_NH3_CURVE_B   (-2.473f)

/* ── R0: clean-air resistance (kΩ) — must be calibrated per unit ────── */
#define MQ135_R0_KOHM       10.0f   /* Default; update after calibration */

/* ── Output clamp ───────────────────────────────────────────────────── */
#define MQ135_PPM_MAX       1000

/* ── Module state ───────────────────────────────────────────────────── */
static adc_oneshot_unit_handle_t s_adc_handle = NULL;
static int64_t                   s_init_time_us = 0;

/* ─────────────────────────────────────────────────────────────────────── */

esp_err_t air_sensor_init(void)
{
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = MQ135_ADC_UNIT,
    };
    ESP_RETURN_ON_ERROR(adc_oneshot_new_unit(&unit_cfg, &s_adc_handle),
                        TAG, "ADC unit init failed");

    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = MQ135_ADC_BITWIDTH,
        .atten    = MQ135_ADC_ATTEN,
    };
    ESP_RETURN_ON_ERROR(adc_oneshot_config_channel(s_adc_handle, MQ135_ADC_CHANNEL, &chan_cfg),
                        TAG, "ADC channel config failed");

    s_init_time_us = esp_timer_get_time();
    ESP_LOGI(TAG, "MQ-135 initialized on GPIO2 (ADC1_CH2), R0=%.1f kΩ, warmup %d ms",
             (double)MQ135_R0_KOHM, AIR_SENSOR_WARMUP_MS);
    return ESP_OK;
}

esp_err_t air_sensor_read(air_sensor_data_t *out)
{
    if (!out)          return ESP_ERR_INVALID_ARG;
    if (!s_adc_handle) return ESP_ERR_INVALID_STATE;

    int64_t elapsed_ms = (esp_timer_get_time() - s_init_time_us) / 1000LL;
    out->is_warming_up = (elapsed_ms < AIR_SENSOR_WARMUP_MS);

    /* Read raw ADC */
    int raw = 0;
    esp_err_t ret = adc_oneshot_read(s_adc_handle, MQ135_ADC_CHANNEL, &raw);
    if (ret != ESP_OK) {
        out->is_valid = false;
        out->nh3_ppm  = 0;
        out->raw_adc  = 0;
        ESP_LOGW(TAG, "ADC read error: %s", esp_err_to_name(ret));
        return ret;
    }
    out->raw_adc = (uint32_t)raw;

    /* raw → voltage (V) */
    float voltage = (raw / 4095.0f) * MQ135_VCC;
    if (voltage < 0.01f) voltage = 0.01f;   /* guard division by zero */

    /* voltage → Rs (kΩ) */
    float rs_kohm = MQ135_LOAD_RESISTANCE_KOHM * (MQ135_VCC - voltage) / voltage;
    if (rs_kohm <= 0.0f) rs_kohm = 0.01f;

    /* Rs/R0 → NH₃ ppm */
    float ratio = rs_kohm / MQ135_R0_KOHM;
    float ppm_f = MQ135_NH3_CURVE_A * powf(ratio, MQ135_NH3_CURVE_B);
    if (ppm_f < 0.0f)          ppm_f = 0.0f;
    if (ppm_f > MQ135_PPM_MAX) ppm_f = (float)MQ135_PPM_MAX;

    out->nh3_ppm  = (uint16_t)ppm_f;
    out->is_valid = true;

    ESP_LOGD(TAG, "raw=%"PRIu32" V=%.3f Rs=%.2fkΩ Rs/R0=%.2f NH3=%.1fppm%s",
             out->raw_adc, (double)voltage, (double)rs_kohm, (double)ratio, (double)ppm_f,
             out->is_warming_up ? " [WARMUP]" : "");

    return ESP_OK;
}
