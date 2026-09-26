/**
 * @file        QRS_PanTompk.c
 * @author      your name (you@domain.com)
 * @brief       This module contains the filter preprocessing functions that are used for QRS detection
 * @version 0.1
 * @date 2026-09-09
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include <stdint.h>
#include <float.h>
#include <string.h>

#include "esp_dsp.h"
#include "dsps_biquad.h"

// TODO: put this into a shared common header file

#define     NUM_CHANNELS        (4)
#define     NUM_SAMPLES         (215)       // 4 * 215 = 860 SPS


// Allocate the biquad delay line (must be initialized to 0)
float lpf_delay[2] = {0.0f, 0.0f};

// Coefficients for a stable 11Hz Low-Pass Filter @ 200Hz sampling rate
// Format: {b0, b1, b2, a1, a2} -> Note: ESP-DSP expects normalized coefficients
float lpf_coeffs[5] = {
    0.0244f, 0.0488f, 0.0244f,  // b0, b1, b2 (Numerator)
    -1.6044f, 0.7020f           // a1, a2     (Denominator)
};

 /**
  * @brief  The Low-Pass Filter per Pan and Tompkins, QRS Detect. This will use recommendations from AI because of
  * some of the unique characteristics that are associated with the ESP32 based ECG monitoring system. For one, the 
  * sampling rate of 215 Hz differs from the 200 Hz sampling rate that Pan and Tompkins (for brevity, I will shorten
  * it to "PT") used for their development.
  * 
  */
 esp_err_t QRS_PT_LowPass( float *inputBuff, float *outputBuff, int  sample_count)
 {

    esp_err_t retv = ESP_OK;

    dsps_biquad_f32(inputBuff, outputBuff, sample_count, lpf_coeffs, lpf_delay);  

    return retv;
 }