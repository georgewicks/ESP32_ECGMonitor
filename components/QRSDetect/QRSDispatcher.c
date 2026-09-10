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
#include <string.h>

// TODO: Must put into common shared header file!

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

float               PrepBuffer[NUM_SAMPLES];            // convert to floating pt.
const               float FS = 4.096;                   // voltage range
ECG_adc_sample_t    ResBuffer;

int     DoQRSPreProcess(float *ppbuf, int nsamp, int channel);

void  PushToECGProc(ECG_adc_sample_t *curECGptr)
{
    int     i;
    int     errv;

    // Prep channel 0
    for(i=0;i<<NUM_SAMPLES;i++)
    {
        PrepBuffer[i] = (float)(curECGptr->samples[0][i]/32768) * FS;
    }
    errv = DoQRSPreProcess(PrepBuffer,NUM_SAMPLES, 0);

    // Prep channel 0
    for( i=0 ; i<NUM_SAMPLES ; i++)
    {
        PrepBuffer[i] = (float)(curECGptr->samples[0][i]/32768) * FS;
    }
    errv = DoQRSPreProcess(PrepBuffer,NUM_SAMPLES, 1);

    // Prep channel 0
    for( i=0 ; i<NUM_SAMPLES ; i++)
    {
        PrepBuffer[i] = (float)(curECGptr->samples[0][i]/32768) * FS;
    }
    errv = DoQRSPreProcess(PrepBuffer,NUM_SAMPLES, 2);

    // Prep channel 0
    for( i=0 ; i<NUM_SAMPLES ; i++)
    {
        PrepBuffer[i] = (float)(curECGptr->samples[0][i]/32768) * FS;
    }
    errv = DoQRSPreProcess(PrepBuffer,NUM_SAMPLES, 3);

    // Now, convert the 4 channel results back to acceptable ISHNE 16-bit 
    // integers (see ISHNE docs)

}

int     DoQRSPreProcess(float *ppbuf, int nsamp, int channel)
{
    int     eval = 0;
    assert(channel >= 0 && channel < 4);

    // phase 1: low-pass filter

    // phase 2: hi-pass filter

    // phase 3: derivative filter

    // phase 4: squaring

    // phase 5: moving-average smoothing


    return(eval);
}