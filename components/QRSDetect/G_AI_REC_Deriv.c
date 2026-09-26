/**
 * @file    G_AI_REC_Deriv.c
 * @author  george.r.wicks@gmail.com
 * @brief   Based on "AI Insight on Digital Filter Design ECG Monitor" document. This was the result of inquiries
 * on Pan and Tompkins "Biomedical Digital Signal Processing" for the QRS Detection algorithm.This contains both 
 * the integer and floating point versions of the Derivative filter. 
 * @version 0.1
 * @date 2026-09-10
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include <stdint.h>
#include <float.h>
#include <string.h>

typedef struct {
    int32_t history[5]; // Stores x[n] through x[n-4]
    float f_history[5];
} pt_deriv_t;

// Integer Derivative
int32_t pt_deriv_int_process(pt_deriv_t *f, int32_t x_in) {
    // Shift history
    f->history[4] = f->history[3];
    f->history[3] = f->history[2];
    f->history[2] = f->history[1];
    f->history[1] = f->history[0];
    f->history[0] = x_in;
    
    // y[n] = (2*x[n] + x[n-1] - x[n-3] - 2*x[n-4]) / 8
    int32_t y = (f->history[0] << 1) + f->history[1] - f->history[3] - (f->history[4] << 1);
    return y >> 3; 
}

// Floating-Point Derivative
float pt_deriv_float_process(pt_deriv_t *f, float x_in) {
    f->f_history[4] = f->f_history[3];
    f->f_history[3] = f->f_history[2];
    f->f_history[2] = f->f_history[1];
    f->f_history[1] = f->f_history[0];
    f->f_history[0] = x_in;
    
    return (2.0f * f->f_history[0] + f->f_history[1] - f->f_history[3] - 2.0f * f->f_history[4]) / 8.0f;
}
