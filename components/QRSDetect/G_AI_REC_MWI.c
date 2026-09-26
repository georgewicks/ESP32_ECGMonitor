/**
 * @file    AI_REC_MWI.c
 * @author  george.r.wicks@gmail.com
 * @brief   Based on "AI Insight on Digital Filter Design ECG Monitor" document. This was the result of inquiries
 * on Pan and Tompkins "Biomedical Digital Signal Processing" for the QRS Detection algorithm.This is the Moving
 * Window Integrator, both integer and floating point versions.
 * @version 0.1
 * @date 2026-09-10
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include <stdint.h>
#include <stdbool.h>

#define MWI_BUFFER_SIZE 32

typedef struct {
    int64_t history[MWI_BUFFER_SIZE]; // Use 64-bit to prevent overflow during squaring
    int head;
    int64_t sum;
    
    float f_history[MWI_BUFFER_SIZE];
} pt_mwi_t;

// Integer Integrator (Expects squared input, outputs smoothed envelope)
int32_t pt_mwi_int_process(pt_mwi_t *f, int32_t x_in) {
    // 1. Square the input signal to obtain power/energy profile
    int64_t squared = (int64_t)x_in * (int64_t)x_in;
    
    // 2. Standard moving average update
    int64_t old_squared = f->history[f->head];
    f->sum = f->sum + squared - old_squared;
    f->history[f->head] = squared;
    
    f->head = (f->head + 1) % MWI_BUFFER_SIZE;
    
    // 3. Average over 32 samples (using fast shift)
    return (int32_t)(f->sum >> 5);
}

// Floating-Point Integrator 
float pt_mwi_float_process(pt_mwi_t *f, float x_in) {
    float squared = x_in * x_in;
    f->f_history[f->head] = squared;
    
    float sum = 0.0f;
    for (int i = 0; i < MWI_BUFFER_SIZE; i++) {
        sum += f->f_history[i];
    }
    
    f->head = (f->head + 1) % MWI_BUFFER_SIZE;
    
    return sum / 32.0f;
}
