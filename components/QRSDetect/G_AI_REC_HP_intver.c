/**
 * @file    G_AI_REC_intver_HP.c
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
#include <string.h>
#define HP_BUFFER_SIZE 32
#define HP_DELAY_TAPS  16

typedef struct {
    int32_t history[HP_BUFFER_SIZE];
    int head;
    int32_t sum;
} pt_hp_int_t;

void pt_hp_int_init(pt_hp_int_t *f) {
    for (int i = 0; i < HP_BUFFER_SIZE; i++) f->history[i] = 0;
    f->head = 0;
    f->sum = 0;
}

int32_t pt_hp_int_process(pt_hp_int_t *f, int32_t x_in) {
    int32_t x_old = f->history[f->head];
    f->sum = f->sum + x_in - x_old;
    f->history[f->head] = x_in;
    
    int delay_idx = (f->head + HP_DELAY_TAPS) % HP_BUFFER_SIZE;
    int32_t x_delayed = f->history[delay_idx];
    
    f->head = (f->head + 1) % HP_BUFFER_SIZE;
    
    // Sum divided by 32 via arithmetic right-shift
    return x_delayed - (f->sum >> 5);
}
