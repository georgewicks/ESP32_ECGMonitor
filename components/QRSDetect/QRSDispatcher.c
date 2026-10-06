/**
 * @file QRSDispatcher.c
 * @author  george.r.wicks@gmail.com
 * @brief  Receives a block of ADC samples and runs QRS detection per channel.
 * @version 0.2
 * @date 2026-10-06
 *
 * @copyright Copyright (c) 2026
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "pan_tompkins.h"

/* TODO: This type duplicates ADCCapture.h (main/ADCCapture.h). Move to a
 * shared header and include it from both components to avoid drift. */
#define NUM_CHANNELS    (4)
#define NUM_SAMPLES     (215)   /* samples per channel per block */

typedef struct {
    uint64_t    micro_timestamp;
    uint16_t    samples[NUM_CHANNELS][NUM_SAMPLES];
} ECG_adc_sample_t;

/* ADS1115 configured for PGA = +/-2.048 V, 16-bit signed result. */
#define ADS_FS_VOLTS    (2.048f)

static pt_context_t  s_ctx[NUM_CHANNELS];
static bool          s_init = false;
static pt_beat_t     s_beats[8];

void PushToECGProc(ECG_adc_sample_t *curECGptr)
{
    if (!curECGptr) {
        return;
    }

    if (!s_init) {
        for (int ch = 0; ch < NUM_CHANNELS; ch++) {
            pan_tompkins_init(&s_ctx[ch]);
        }
        s_init = true;
    }

    float prep[NUM_SAMPLES];

    for (int ch = 0; ch < NUM_CHANNELS; ch++) {
        for (int i = 0; i < NUM_SAMPLES; i++) {
            /* Raw ADC codes are unsigned two's-complement halves of the
             * 16-bit range: cast to signed then scale to volts. */
            prep[i] = (float)((int16_t)curECGptr->samples[ch][i])
                      * (ADS_FS_VOLTS / 32768.0f);
        }

        int n = pan_tompkins_process(&s_ctx[ch], prep, NUM_SAMPLES,
                                     s_beats, sizeof(s_beats) / sizeof(s_beats[0]));
        for (int b = 0; b < n; b++) {
            printf("QRS ch%d: R-peak at sample %lu, RR = %lu ms\n",
                   ch, (unsigned long)s_beats[b].index,
                   (unsigned long)s_beats[b].rr_ms);
        }
    }
}
