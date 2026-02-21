/*
 * SPDX-FileCopyrightText: 2024 Reasty
 *
 * SPDX-License-Identifier: CC0-1.0
 *
 * event_detector.h — 3-state NH₃ event detector for LitterBox.v1
 *
 * State machine: IDLE → ACTIVE → COOLDOWN → IDLE
 *  - IDLE:     tracking baseline via EMA; transitions to ACTIVE on ppm spike
 *  - ACTIVE:   tracking peak and duration; transitions to COOLDOWN when ppm
 *              drops back to baseline for EVENT_END_TICKS consecutive ticks
 *  - COOLDOWN: holds classified event type for EVENT_COOLDOWN_TICKS ticks,
 *              then returns to IDLE
 *
 * Event classification (at ACTIVE→COOLDOWN transition):
 *  peak_ticks ≤ URINE_FAST_PEAK_TICKS  OR  peak_delta > 30 ppm  → URINATION
 *  otherwise                                                       → DEFECATION
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

/* ---------- Tunable constants ---------- */
#define EVENT_TRIGGER_DELTA_PPM   10.0f  /* Baseline + this → ACTIVE */
#define EVENT_HYSTERESIS_PPM       3.0f  /* Baseline + this → "near baseline" */
#define EVENT_END_TICKS            3     /* Consecutive near-baseline ticks → end */
#define EVENT_COOLDOWN_TICKS       6     /* Post-event cooldown ticks (6 × 10s = 60s) */
#define URINE_FAST_PEAK_TICKS      3     /* Peak within 3 ticks (30s) → URINATION */
#define BASELINE_ALPHA             0.05f /* EMA coefficient (~200s time constant) */

/* ---------- Types ---------- */

typedef enum {
    LITTER_EVENT_NONE       = 0,
    LITTER_EVENT_URINATION  = 1,
    LITTER_EVENT_DEFECATION = 2,
} litter_event_t;

typedef enum {
    DETECTOR_IDLE,
    DETECTOR_ACTIVE,
    DETECTOR_COOLDOWN,
} detector_state_t;

typedef struct {
    float            baseline_ppm;
    detector_state_t state;
    litter_event_t   current_event;
    float            peak_ppm;
    uint16_t         event_ticks;     /* Ticks since ACTIVE started */
    uint16_t         peak_ticks;      /* Tick at which peak was reached */
    uint8_t          below_thresh_count; /* Consecutive ticks near baseline */
    uint8_t          cooldown_ticks;  /* Ticks spent in COOLDOWN */
    bool             initialized;     /* False until first ppm reading sets baseline */
} event_detector_t;

/* ---------- API ---------- */

/**
 * @brief Initialize (or reset) the detector context.
 *        Call once before the first event_detector_update().
 */
void event_detector_init(event_detector_t *ctx);

/**
 * @brief Feed one ppm reading into the state machine.
 *
 * @param ctx  Detector context (must be initialized)
 * @param ppm  Current NH₃ reading in ppm
 * @return     Current litter_event_t value that the ZCL attribute should hold.
 *             LITTER_EVENT_NONE while idle or still active (unclassified).
 *             LITTER_EVENT_URINATION / DEFECATION while in COOLDOWN.
 *             Transitions back to LITTER_EVENT_NONE when COOLDOWN ends.
 */
litter_event_t event_detector_update(event_detector_t *ctx, float ppm);

/**
 * @brief Return the current estimated baseline ppm.
 */
float event_detector_get_baseline(const event_detector_t *ctx);
