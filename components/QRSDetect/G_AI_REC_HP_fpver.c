/**
 * @file    G_AI_REC_HP_fpver.c
 * @author  george.r.wicks@gmail.com
 * @brief   Based on "AI Insight on Digital Filter Design ECG Monitor" document. This was the result of inquiries
 * on Pan and Tompkins "Biomedical Digital Signal Processing" for the QRS Detection algorithm.This is the integer 
 * version of the High Pass filter. NOTE: combining low-pass and high-pass filters will give the bandpass filter
 * @version 0.1
 * @date 2026-09-10
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include <stdint.h> 
#include <float.h>
#include <string.h>
#define HP_BUFFER_SIZE 32
#define HP_DELAY_TAPS  16

typedef struct {
    float history[HP_BUFFER_SIZE];
    int head;
} pt_hp_float_t;

void pt_hp_float_init(pt_hp_float_t *f) {
    for (int i = 0; i < HP_BUFFER_SIZE; i++) f->history[i] = 0.0f;
    f->head = 0;
}

float pt_hp_float_process(pt_hp_float_t *f, float x_in) {
    f->history[f->head] = x_in;
    
    int delay_idx = (f->head + HP_DELAY_TAPS) % HP_BUFFER_SIZE;
    float x_delayed = f->history[delay_idx];
    
    // Explicit sum loop avoids floating-point integration drift
    float sum = 0.0f;
    for (int i = 0; i < HP_BUFFER_SIZE; i++) {
        sum += f->history[i];
    }
    
    f->head = (f->head + 1) % HP_BUFFER_SIZE;
    
    return x_delayed - (sum / 32.0f);
}