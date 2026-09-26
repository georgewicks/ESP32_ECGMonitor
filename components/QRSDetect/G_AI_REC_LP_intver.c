/**
 * @file    G_AI_REC_LP_intver.c
 * @author  george.r.wicks@gmail.com
 * @brief   Based on "AI Insight on Digital Filter Design ECG Monitor" document. This was the result of inquiries
 * on Pan and Tompkins "Biomedical Digital Signal Processing" for the QRS Detection algorithm.This is the integer 
 * version of the Low Pass filter.
 * @version 0.1
 * @date 2026-09-10
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include <stdint.h>
#include <string.h>

#define LP_BUFFER_SIZE 16 // Needs to hold at least 12 samples

typedef struct {
    int32_t x_history[LP_BUFFER_SIZE];
    int32_t y_history[2]; // Stores y[n-1] and y[n-2]
    int head;
} pt_lp_int_t;

void pt_lp_int_init(pt_lp_int_t *f) {
    for (int i = 0; i < LP_BUFFER_SIZE; i++) f->x_history[i] = 0;
    f->y_history[0] = 0;
    f->y_history[1] = 0;
    f->head = 0;
}

int32_t pt_lp_int_process(pt_lp_int_t *f, int32_t x_in) {
    // 1. Store current input in the ring buffer
    f->x_history[f->head] = x_in;
    
    // 2. Fetch the delayed samples
    int idx_6  = (f->head - 6 + LP_BUFFER_SIZE) % LP_BUFFER_SIZE;
    int idx_12 = (f->head - 12 + LP_BUFFER_SIZE) % LP_BUFFER_SIZE;
    
    int32_t x_6  = f->x_history[idx_6];
    int32_t x_12 = f->x_history[idx_12];
    
    // 3. Compute recursive difference equation
    // y[n] = 2*y[n-1] - y[n-2] + x[n] - 2*x[n-6] + x[n-12]
    int32_t y_out = (f->y_history[0] << 1) - f->y_history[1] + x_in - (x_6 << 1) + x_12;
    
    // 4. Update internal history
    f->y_history[1] = f->y_history[0];
    f->y_history[0] = y_out;
    
    // 5. Advance ring buffer
    f->head = (f->head + 1) % LP_BUFFER_SIZE;
    
    // 6. Scale output for Unity Gain (Original DC gain is 36)
    return y_out / 36;
}

