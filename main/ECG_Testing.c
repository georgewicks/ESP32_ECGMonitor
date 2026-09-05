/**
 * @file Testing.c
 * @author george.r.wicks@gmail.com
 * @brief  We're going to need some basic testing functionality. Tests go here.
 * @version 0.1
 * @date 2026-09-03
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#include <stdio.h>
#include <inttypes.h>
#include <time.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


void basic_test_loop( void )
{
    int count = 0;
    while(1)
    {
        // This is a visual way of confirming that the ADC capture is functioning... or not.
        // TODO: Move this to a basic POST test, that can be invoked for testing.
        // ESP_LOGI(TAG, "Running... SD count = %d, ADC count = %d",sd_signal_count,adc_signal_count);
        vTaskDelay(1000/portTICK_PERIOD_MS);
        count++;
        if(count == 60)
        {
            break;
        }
    }
}

