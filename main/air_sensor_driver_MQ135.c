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
 *  MQ-135 AOUT → GPIO0 (XIAO ESP32-C6 A0 = ADC1_CHANNEL_0)
 *
 *  ⚠ VOLTAGE / WIRING:
 *    MQ-135 VCC  → XIAO VBUS (5 V)
 *    MQ-135 GND  → XIAO GND
 *    MQ-135 AOUT → 100 kΩ → GPIO0 (ADC)
 *                      │
 *                    100 kΩ
 *                      │
 *                     GND
 *    Voltage divider (1:1) halves AOUT: max 5V → 2.5V → safe for ADC (3.3V max).
 *    ADC reference is still 3.3 V; AOUT_actual = V_adc × DIVIDER_RATIO (2.0).
 *
 * ── Electrical model ─────────────────────────────────────────────────────
 *  AOUT_sensor = VCC × RL / (Rs + RL)
 *  V_adc       = AOUT_sensor / DIVIDER_RATIO          [what ADC sees]
 *  AOUT_sensor = V_adc × DIVIDER_RATIO
 *  Rs          = RL × (VCC − AOUT_sensor) / AOUT_sensor
 *  Rs/R0 → NH₃ ppm via sensitivity curve  [R0 = resistance in clean air]
 *
 * ── Calibration ──────────────────────────────────────────────────────────
 *  R0 (MQ135_R0_KOHM) must be measured per sensor unit at 5 V:
 *    1. Power on in clean outdoor air (5 V supply + divider).
 *    2. Wait ≥ 20 minutes for full thermal stabilization.
 *    3. Record raw_adc from serial log.
 *    4. V_adc = raw / 4095 × 3.3
 *       AOUT  = V_adc × 2.0
 *       Rs    = RL × (5.0 − AOUT) / AOUT
 *       R0    = Rs / 3.6
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
#define MQ135_ADC_CHANNEL       ADC_CHANNEL_0   /* GPIO0 = XIAO A0 */
#define MQ135_ADC_ATTEN         ADC_ATTEN_DB_12 /* Input range 0 ~ 3.1 V */
#define MQ135_ADC_BITWIDTH      ADC_BITWIDTH_12  /* 0 ~ 4095 */

/* ── Electrical constants ───────────────────────────────────────────── */
#define MQ135_LOAD_RESISTANCE_KOHM  10.0f   /* RL on module board (kΩ) */
#define MQ135_VCC                   5.0f    /* Sensor supply voltage (V) — VBUS */
#define MQ135_DIVIDER_RATIO         2.0f    /* AOUT voltage divider (100kΩ:100kΩ = ×2) */
#define MQ135_ADC_VREF              3.3f    /* ESP32-C6 ADC reference voltage (V) */

/* ── NH₃ sensitivity curve ──────────────────────────────────────────── */
#define MQ135_NH3_CURVE_A   102.2f
#define MQ135_NH3_CURVE_B   (-2.473f)

/* ── R0: clean-air reference resistance (kΩ) ────────────────────────── */
/* Initial estimate for 5 V operation. Must be recalibrated:
 *   power on outdoors 20+ min, read raw from log, then:
 *   V_adc = raw/4095 × 3.3,  AOUT = V_adc × 2.0
 *   Rs = RL × (5.0 − AOUT) / AOUT,  R0 = Rs / 3.6
 * Typical range at 5 V: 8~20 kΩ */
#define MQ135_R0_KOHM       10.0f

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
    ESP_LOGI(TAG, "MQ-135 initialized on GPIO0 (ADC1_CH0), VCC=%.1fV divider=%.1f R0=%.1f kΩ, warmup %d ms",
             (double)MQ135_VCC, (double)MQ135_DIVIDER_RATIO, (double)MQ135_R0_KOHM, AIR_SENSOR_WARMUP_MS);
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

    /* raw → V_adc (what ADC sees after divider) */
    float v_adc = (raw / 4095.0f) * MQ135_ADC_VREF;
    if (v_adc < 0.001f) v_adc = 0.001f;    /* guard division by zero */

    /* V_adc → AOUT_sensor (actual sensor output, before divider) */
    float voltage = v_adc * MQ135_DIVIDER_RATIO;
    if (voltage >= MQ135_VCC) voltage = MQ135_VCC - 0.01f;

    /* AOUT_sensor → Rs (kΩ) */
    float rs_kohm = MQ135_LOAD_RESISTANCE_KOHM * (MQ135_VCC - voltage) / voltage;
    if (rs_kohm <= 0.0f) rs_kohm = 0.01f;

    /* Rs/R0 → NH₃ ppm */
    float ratio = rs_kohm / MQ135_R0_KOHM;
    float ppm_f = MQ135_NH3_CURVE_A * powf(ratio, MQ135_NH3_CURVE_B);
    if (ppm_f < 0.0f)          ppm_f = 0.0f;
    if (ppm_f > MQ135_PPM_MAX) ppm_f = (float)MQ135_PPM_MAX;

    out->nh3_ppm   = (uint16_t)ppm_f;
    out->nh3_ppm_f = ppm_f;
    out->is_valid  = true;

    ESP_LOGD(TAG, "raw=%"PRIu32" Vadc=%.3f Aout=%.3f Rs=%.2fkΩ Rs/R0=%.2f NH3=%.1fppm%s",
             out->raw_adc, (double)v_adc, (double)voltage, (double)rs_kohm, (double)ratio, (double)ppm_f,
             out->is_warming_up ? " [WARMUP]" : "");

    return ESP_OK;
}
