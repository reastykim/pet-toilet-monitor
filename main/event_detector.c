/*
 * SPDX-FileCopyrightText: 2024 Reasty
 *
 * SPDX-License-Identifier: CC0-1.0
 *
 * event_detector.c — 3-state NH₃ event detector implementation
 */
#include "event_detector.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "DETECTOR";

void event_detector_init(event_detector_t *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->state = DETECTOR_IDLE;
    ctx->current_event = LITTER_EVENT_NONE;
    ctx->initialized = false;
}

litter_event_t event_detector_update(event_detector_t *ctx, float ppm)
{
    /* First reading: initialise baseline, don't trigger */
    if (!ctx->initialized) {
        ctx->baseline_ppm = ppm;
        ctx->initialized = true;
        ESP_LOGI(TAG, "Baseline initialised: %.1f ppm", ctx->baseline_ppm);
        return LITTER_EVENT_NONE;
    }

    switch (ctx->state) {

    /* ── IDLE ─────────────────────────────────────────────────────────────── */
    case DETECTOR_IDLE:
        /* Update baseline via EMA (only in IDLE — don't drift baseline during event) */
        ctx->baseline_ppm = (1.0f - BASELINE_ALPHA) * ctx->baseline_ppm
                          + BASELINE_ALPHA * ppm;

        ESP_LOGD(TAG, "state=IDLE  baseline=%.1f ppm  current=%.1f ppm",
                 ctx->baseline_ppm, ppm);

        if (ppm > ctx->baseline_ppm + EVENT_TRIGGER_DELTA_PPM) {
            ctx->state            = DETECTOR_ACTIVE;
            ctx->peak_ppm         = ppm;
            ctx->event_ticks      = 1;
            ctx->peak_ticks       = 1;
            ctx->below_thresh_count = 0;
            ESP_LOGI(TAG, "Event START: ppm=%.1f  baseline=%.1f  delta=%.1f",
                     ppm, ctx->baseline_ppm, ppm - ctx->baseline_ppm);
        }
        break;

    /* ── ACTIVE ───────────────────────────────────────────────────────────── */
    case DETECTOR_ACTIVE:
        ctx->event_ticks++;

        /* Track peak */
        if (ppm > ctx->peak_ppm) {
            ctx->peak_ppm   = ppm;
            ctx->peak_ticks = ctx->event_ticks;
        }

        /* Hysteresis: count consecutive ticks near baseline */
        if (ppm < ctx->baseline_ppm + EVENT_HYSTERESIS_PPM) {
            ctx->below_thresh_count++;
        } else {
            ctx->below_thresh_count = 0;
        }

        ESP_LOGD(TAG, "state=ACTIVE  tick=%u  ppm=%.1f  peak=%.1f@tick%u  below=%u",
                 ctx->event_ticks, ppm, ctx->peak_ppm,
                 ctx->peak_ticks, ctx->below_thresh_count);

        /* End condition: stayed near baseline for EVENT_END_TICKS consecutive ticks */
        if (ctx->below_thresh_count >= EVENT_END_TICKS) {
            /* Classify: fast peak (≤ URINE_FAST_PEAK_TICKS) or high delta → URINATION */
            bool fast_peak = (ctx->peak_ticks <= URINE_FAST_PEAK_TICKS);
            bool high_peak = ((ctx->peak_ppm - ctx->baseline_ppm) > 30.0f);

            ctx->current_event = (fast_peak || high_peak)
                                ? LITTER_EVENT_URINATION
                                : LITTER_EVENT_DEFECATION;

            ctx->state         = DETECTOR_COOLDOWN;
            ctx->cooldown_ticks = 0;

            ESP_LOGI(TAG, "Event END → %s  (peak=%.1fppm @ tick%u, baseline=%.1fppm, delta=%.1f)",
                     ctx->current_event == LITTER_EVENT_URINATION ? "URINATION" : "DEFECATION",
                     ctx->peak_ppm, ctx->peak_ticks,
                     ctx->baseline_ppm, ctx->peak_ppm - ctx->baseline_ppm);
        }
        break;

    /* ── COOLDOWN ─────────────────────────────────────────────────────────── */
    case DETECTOR_COOLDOWN:
        ctx->cooldown_ticks++;

        ESP_LOGD(TAG, "state=COOLDOWN  %u/%u ticks", ctx->cooldown_ticks, EVENT_COOLDOWN_TICKS);

        if (ctx->cooldown_ticks >= EVENT_COOLDOWN_TICKS) {
            ctx->state         = DETECTOR_IDLE;
            ctx->current_event = LITTER_EVENT_NONE;
            ESP_LOGI(TAG, "Cooldown complete — returning to IDLE");
        }
        break;
    }

    return ctx->current_event;
}

float event_detector_get_baseline(const event_detector_t *ctx)
{
    return ctx->baseline_ppm;
}
