/**
 * @file pan_tompkins.h
 * @brief Pan-Tompkins real-time QRS detection (streaming, per-channel state).
 *
 * Implements the classic Pan & Tompkins (1985) preprocessing chain:
 *   60 Hz mains notch -> bandpass (cascaded Butterworth LP @ 11 Hz + HP @ 5 Hz)
 *   -> derivative -> squaring -> moving-window integrator (150 ms) ->
 *   adaptive dual-threshold decision with T-wave rejection.
 *
 * One `pt_context_t` must be maintained per ECG channel (the channels are
 * time-multiplexed, so filter/detection state must not be shared).
 *
 * The coefficient set below is designed for a 357 Hz per-channel sample rate
 * (dual ADS1115, ~1429 SPS total across 4 channels).
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PT_FS_HZ        357.0f  /* per-channel sample rate (Hz)             */
#define PT_MWI_WINDOW   54      /* 150 ms moving-window integrator length   */
#define PT_REFRACTORY   71      /* 200 ms refractory period (samples)       */
#define PT_LEARN        714     /* 2 s learning phase (samples)             */

/** One detected heartbeat. */
typedef struct {
    uint32_t index;   /* absolute sample index of the R peak (monotonic) */
    uint32_t rr_ms;   /* RR interval to previous beat, in ms (0 = first) */
} pt_beat_t;

/** Per-channel detector state (zero-initialize before use). */
typedef struct {
    /* 60 Hz notch filter state (direct-form II) */
    float notch_x1, notch_x2, notch_y1, notch_y2;
    /* bandpass biquad state (direct-form II) */
    float lp_x1, lp_x2, lp_y1, lp_y2;
    float hp_x1, hp_x2, hp_y1, hp_y2;
    /* derivative history: x[n-1] .. x[n-4] */
    float d1, d2, d3, d4;
    /* moving-window integrator */
    float mwi_buf[PT_MWI_WINDOW];
    int   mwi_head;
    float mwi_sum;
    /* learning phase */
    float learn_max;
    /* adaptive thresholds */
    float spki, npki, thr1, thr2;
    /* running sample index (absolute, across blocks) */
    uint32_t index;
    /* peak tracking */
    float    peak_amp;
    uint32_t peak_index;
    uint32_t last_peak_index;
    /* last QRS */
    uint32_t last_qrs_index;   /* 0 => no beat detected yet */
    float    last_qrs_amp;
} pt_context_t;

/** Zero a detector context. */
void pan_tompkins_init(pt_context_t *ctx);

/**
 * Process a block of ECG samples for one channel.
 *
 * @param ctx       per-channel state (initialized once)
 * @param x         input samples (volts; DC is removed by the bandpass)
 * @param count     number of samples in @p x
 * @param beats     output array for detected beats
 * @param max_beats capacity of @p beats
 * @return number of beats written to @p beats
 */
int pan_tompkins_process(pt_context_t *ctx, const float *x, int count,
                         pt_beat_t *beats, int max_beats);

#ifdef __cplusplus
}
#endif
