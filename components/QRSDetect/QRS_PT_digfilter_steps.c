/**
 * @file    QRS_PT_digfilter_steps.c
 * @author  george.r.wicks@gmail.com
 * @brief   This is part of the Pan-Tompkins QRS Detection algorithm, adapted for the ESP32 platform, and the use of the ADS1115 ADC chip. We 
 * are using the Espressif ESP-DSP library. There are several recommendations from Google AI, particularly the use of the ESP-DSP component, but 
 * also the changes needed for 215 Hz sample rate per channel for the maximum 860 samples per second (SPS). 215 Hz is the closest to the original
 * 200 Hz rate that was implemented for the Pan-Tompkins algorithm, and thus needed some fine tuning for this platform.
 * @version 0.1
 * @date    2026-09-07
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"

#include "esp_dsp.h"
#include "dsps_biquad.h"

/* --------------------------------------------------------------------------------------------- */
/* These are recommendations from Google AI */
/* --------------------------------------------------------------------------------------------- */

// IMPORTANT: You must maintain a SEPARATE delay line buffer for EACH ECG channel.
// Because the channels are multiplexed, sharing a delay buffer will corrupt the data.
float lpf_delay_ch0[2] = {0.0f, 0.0f}; 
float lpf_delay_ch1[2] = {0.0f, 0.0f};
float lpf_delay_ch2[2] = {0.0f, 0.0f};

// Optimized coefficients for 11Hz Cutoff @ 215Hz Sampling Rate
// Format: {b0, b1, b2, a1, a2} -> (Note: ESP-DSP expects a1 and a2 to be normalized)
const float lpf_coeffs_215hz[5] = {
    0.02094f, 0.04187f, 0.02094f,  // b0, b1, b2 (Numerator)
    -1.55102f, 0.63477f            // a1, a2     (Denominator)
};

// From AI Example of how to use the library function
// 2. Processing the ECG Signal Loop 
//   When you receive your raw ECG ADC samples, you pass them through the optimized 
//  biquad function block by block or sample by sample.
c
void process_ecg_sample(float *raw_ecg_buffer, float *filtered_ecg_buffer, int sample_count) 
{
    // dsps_biquad_f32 processes arrays. For real-time single samples, set sample_count to 1.
    dsps_biquad_f32(raw_ecg_buffer, filtered_ecg_buffer, sample_count, lpf_coeffs, lpf_delay);
}

// This is from the analysis of the 215 Hz rate from the ADS1115 (see notes in book).
// Running the Filter in Your Loop
//  When reading your channels sequentially from the ADS1115, pass the sample into dsps_biquad_f32 
// using that specific channel's unique delay line:

// Example processing loop
float raw_sample_ch0 = read_ads1115_channel(0); // Convert your int16 to float voltage
float filtered_sample_ch0 = 0.0f;

// Process 1 sample at a time (sample_count = 1)
dsps_biquad_f32(&raw_sample_ch0, &filtered_sample_ch0, 1, lpf_coeffs_215hz, lpf_delay_ch0);

//  Use code with caution.  --- as always!
