/**
 * @file QRSDispatcher.c
 * @author  george.r.wicks@gmail.com
 * @brief  Handles the incoming data packet for analysis
 * @version 0.1
 * @date 2026-09-04
 * 
 * @copyright Copyright (c) 2026
 * 
 */

 #include <stdint.h>

/**
 * @brief  ADC samples for ECG capture
 * 
 */
#define     NUM_CHANNELS        (4)
#define     NUM_SAMPLES         (215)       // 4 * 215 = 860 SPS

typedef struct {
    uint64_t    micro_timestamp;
    uint16_t    samples[NUM_CHANNELS][NUM_SAMPLES];
} ECG_adc_sample_t;


void  PushToECGProc(ECG_adc_sample_t *curECGptr)
{


}
