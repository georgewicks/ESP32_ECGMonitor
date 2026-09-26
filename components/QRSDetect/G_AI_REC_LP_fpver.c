/**
 * @file    G_AI_REC_LP_fpver.c
 * @author  george.r.wicks@gmail.com
 * @brief   Based on "AI Insight on Digital Filter Design ECG Monitor" document. This was the result of inquiries
 * on Pan and Tompkins "Biomedical Digital Signal Processing" for the QRS Detection algorithm. This is the floating
 * point verion of the Low Pass filter.
 * @version 0.1
 * @date 2026-09-10
 * 
 * @copyright Copyright (c) 2026
 * 
 */

/*
Option B: Floating-Point / ESP-DSP FIR Workaround
If you must use floating-point or want to leverage the ESP-DSP library blocks safely, you cannot 
use the recursive IIR form because of the drift hazard. Instead, you must expand the transfer 
function algebraically into its pure FIR form.

By performing polynomial division, the filter expands to a 13-tap symmetric FIR filter with a triangular impulse response:

(y[n]=x[n]+2x[n-1]+3x[n-2]+4x[n-3]+5x[n-4]+6x[n-5]+7x[n-6]+6x[n-7]+5x[n-8]+4x[n-9]+3x[n-10]+2x[n-11]+x[n-12]\)

This FIR form contains no feedback loops, making it 100% stable and perfectly suited for ESP-DSP's block processing.
 */

#include <stdint.h>
#include <float.h>
#include <string.h>


#include "esp_dsp.h"
#include "dsps_fir.h"

#define LP_FIR_TAPS 13
#define BLOCK_SIZE  32 // Assumes your ping-pong buffer block size

float lp_fir_coeffs[LP_FIR_TAPS];
float lp_fir_delay_line[LP_FIR_TAPS];
fir_f32_t lp_fir;

void init_espdsp_lowpass() {
    // Triangular weights divided by 36 to enforce unity gain natively
    float weights[LP_FIR_TAPS] = {
        1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 6.0f, 5.0f, 4.0f, 3.0f, 2.0f, 1.0f
    };
    
    for (int i = 0; i < LP_FIR_TAPS; i++) {
        lp_fir_coeffs[i] = weights[i] / 36.0f;
    }

    // Initialize ESP-DSP FIR structure
    dsps_fir_init_f32(&lp_fir, lp_fir_coeffs, lp_fir_delay_line, LP_FIR_TAPS);
}

// Called inside your buffer processing loop
void process_lp_with_espdsp(float *input_block, float *output_block) {
    dsps_fir_f32(&lp_fir, input_block, output_block, BLOCK_SIZE);
}