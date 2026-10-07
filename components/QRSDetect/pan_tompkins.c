/**
 * @file pan_tompkins.c
 * @brief Pan-Tompkins QRS detector implementation (streaming).
 *
 * The difference equations and decision rules follow Pan & Tompkins, "A
 * Real-Time QRS Detection Algorithm", IEEE TBME, 1985, adapted to a 357 Hz
 * sample rate. Coefficients were computed offline (see tools/). The decision
 * stage uses adaptive thresholds SPKI/NPKI with T-wave rejection.
 *
 * A 60 Hz mains notch filter is applied ahead of the bandpass. Its center is
 * computed for 352 Hz (the observed per-channel rate; the nominal 357 Hz
 * drifts ~2% with bus contention), which gives robust rejection across the
 * observed 349-356 Hz range.
 */

#include <stdbool.h>
#include <string.h>

#include "pan_tompkins.h"

/* 60 Hz mains notch, 2nd-order IIR (scipy.signal.iirnotch, Fs=352 Hz, Q=10). */
#define NOTCH_B0  0.949126f
#define NOTCH_B1 -0.909735f
#define NOTCH_B2  0.949126f
#define NOTCH_A1 -0.909735f
#define NOTCH_A2  0.898252f

/* Butterworth 2nd-order low-pass @ 11 Hz, Fs = 357 Hz (scipy.signal.butter) */
#define LP_B0  0.008222f
#define LP_B1  0.016445f
#define LP_B2  0.008222f
#define LP_A1 -1.727609f
#define LP_A2  0.760498f

/* Butterworth 2nd-order high-pass @ 5 Hz, Fs = 357 Hz */
#define HP_B0  0.939670f
#define HP_B1 -1.879339f
#define HP_B2  0.939670f
#define HP_A1 -1.875696f
#define HP_A2  0.882982f

void pan_tompkins_init(pt_context_t *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
}

int pan_tompkins_process(pt_context_t *ctx, const float *x, int count,
                         pt_beat_t *beats, int max_beats)
{
    int nbeats = 0;

    for (int i = 0; i < count; i++) {
        const float v = x[i];

        /* --- 60 Hz notch (direct-form II), ahead of the bandpass --- */
        const float nz = NOTCH_B0 * v + NOTCH_B1 * ctx->notch_x1 + NOTCH_B2 * ctx->notch_x2
                       - NOTCH_A1 * ctx->notch_y1 - NOTCH_A2 * ctx->notch_y2;
        ctx->notch_x2 = ctx->notch_x1;
        ctx->notch_x1 = v;
        ctx->notch_y2 = ctx->notch_y1;
        ctx->notch_y1 = nz;

        /* --- low-pass biquad (direct-form II) --- */
        const float lp = LP_B0 * nz + LP_B1 * ctx->lp_x1 + LP_B2 * ctx->lp_x2
                       - LP_A1 * ctx->lp_y1 - LP_A2 * ctx->lp_y2;
        ctx->lp_x2 = ctx->lp_x1;
        ctx->lp_x1 = nz;
        ctx->lp_y2 = ctx->lp_y1;
        ctx->lp_y1 = lp;

        /* --- high-pass biquad (input = low-pass output) --- */
        const float hp = HP_B0 * lp + HP_B1 * ctx->hp_x1 + HP_B2 * ctx->hp_x2
                       - HP_A1 * ctx->hp_y1 - HP_A2 * ctx->hp_y2;
        ctx->hp_x2 = ctx->hp_x1;
        ctx->hp_x1 = lp;
        ctx->hp_y2 = ctx->hp_y1;
        ctx->hp_y1 = hp;

        /* --- derivative: (2x[n] + x[n-1] - x[n-3] - 2x[n-4]) / 8 --- */
        const float der = (2.0f * hp + ctx->d1 - ctx->d3 - 2.0f * ctx->d4) * 0.125f;
        ctx->d4 = ctx->d3;
        ctx->d3 = ctx->d2;
        ctx->d2 = ctx->d1;
        ctx->d1 = hp;

        /* --- squaring --- */
        const float sq = der * der;

        /* --- moving-window integrator (150 ms) --- */
        ctx->mwi_sum += sq - ctx->mwi_buf[ctx->mwi_head];
        ctx->mwi_buf[ctx->mwi_head] = sq;
        ctx->mwi_head = (ctx->mwi_head + 1) % PT_MWI_WINDOW;
        const float mwi = ctx->mwi_sum / (float)PT_MWI_WINDOW;

        const uint32_t idx = ctx->index++;

        /* --- learning phase: initialize thresholds from the first 2 s --- */
        if (idx < PT_LEARN) {
            if (mwi > ctx->learn_max) {
                ctx->learn_max = mwi;
            }
            if (idx == PT_LEARN - 1) {
                ctx->spki = ctx->learn_max;
                ctx->npki = 0.5f * ctx->learn_max;
                ctx->thr1 = ctx->npki + 0.25f * (ctx->spki - ctx->npki);
                ctx->thr2 = 0.5f * ctx->thr1;
            }
            continue;
        }

        /* --- peak tracking (respect refractory period) --- */
        if (idx - ctx->last_peak_index >= PT_REFRACTORY) {
            if (mwi > ctx->peak_amp) {
                ctx->peak_amp = mwi;
                ctx->peak_index = idx;
            }
            /* A peak is complete once the integrator falls to half its max. */
            if (mwi < 0.5f * ctx->peak_amp && ctx->peak_amp > 0.0f) {
                const float    amp = ctx->peak_amp;
                const uint32_t pi  = ctx->peak_index;
                ctx->peak_amp = 0.0f;

                /* T-wave rejection: within 360 ms of last QRS and
                 * amplitude below half the previous QRS amplitude. */
                const bool twave = (ctx->last_qrs_index != 0 &&
                                    (pi - ctx->last_qrs_index) < (uint32_t)(0.36f * PT_FS_HZ) &&
                                    amp < 0.5f * ctx->last_qrs_amp);

                if (amp > ctx->thr1 && !twave) {
                    uint32_t rr = 0;
                    if (ctx->last_qrs_index != 0) {
                        rr = (uint32_t)((pi - ctx->last_qrs_index) * 1000.0f / PT_FS_HZ);
                    }
                    if (nbeats < max_beats) {
                        beats[nbeats].index = pi;
                        beats[nbeats].rr_ms = rr;
                        nbeats++;
                    }
                    ctx->last_qrs_index = pi;
                    ctx->last_qrs_amp = amp;
                    ctx->spki = 0.125f * amp + 0.875f * ctx->spki;
                } else {
                    ctx->npki = 0.125f * amp + 0.875f * ctx->npki;
                }

                ctx->thr1 = ctx->npki + 0.25f * (ctx->spki - ctx->npki);
                ctx->thr2 = 0.5f * ctx->thr1;
                ctx->last_peak_index = pi;
            }
        }
    }

    return nbeats;
}
