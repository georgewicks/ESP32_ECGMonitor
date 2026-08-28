/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"

// AI suggested additions
#include "esp_timer.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

#include <esp_log.h>


#include "spoof_i2cdev.h"

#include "common.h"

#include "ADCCapture.h"

static const char *TAG = "main";

extern void     i2c_master_init(void);
extern void     InitI2C(void);
extern bool     IsADS1115Present(void);
extern bool     IsDS3231Present(void);

// if we decide to use HTTP, we might need to use this to avoid I2C timeouts
SemaphoreHandle_t	xMutex;

i2c_master_bus_handle_t bus_handle;
i2c_master_dev_handle_t dev_handle;


extern esp_err_t ads111x_read_reg( i2c_dev_t *dev, uint8_t reg, uint16_t *val);
extern esp_err_t ads111x_write_reg(i2c_dev_t *dev, uint8_t reg, uint16_t val);

extern i2c_dev_t	ECGDev_ads1115;
extern esp_err_t	InitADS1115(void);

static QueueHandle_t adc_queue = NULL;
static SemaphoreHandle_t ads_rdy_semaphore = NULL;



// Exact configuration for the ADC:   Specify the channel (0-3), PGA = 010b, Single-shot (in conjunction with the ALRT pin, 
// which triggers an interrupt), 860 SPS (DR field, 111b), ... TODO: better bit map description, and maybe setup bitmasks with shifts, etc. 
// NOTE: There are indeed very good reasons for consilidating all the bits together in a single 16-bit value to be written to the 
// configuration register: The API calls for writing to the configuration register can describe exactly the bitfields that make up
// the given field, but at a cost: the API call will first read the configuration value, then mask the register value with the shifted
// bitfield, and then write it back out to the configuration register. Using an atomic constant write operation makes the call to the 
// ADS1115 fast and efficient, which is good goal, since we want to insure that the capture process is time intensive.
static const uint16_t ads_mux_config[4] = {
    0xC4E8, // Ch 0, Single-Shot, 860 SPS, Alert Active Low
    0xD4E8, // Ch 1, Single-Shot, 860 SPS, Alert Active Low
    0xE4E8, // Ch 2, Single-Shot, 860 SPS, Alert Active Low
    0xF4E8  // Ch 3, Single-Shot, 860 SPS, Alert Active Low
};

// Global wall clock shared safely via thread updates
static volatile rtc_time_t global_wall_clock = {0};

static void     do_restart(void)
{
    ESP_LOGI(TAG, "%s: called", __func__);

    for (int i = 100; i >= 0; i--) {
        printf("Restarting in %d seconds...\n", i);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    printf("Restarting now.\n");
    fflush(stdout);
    esp_restart();

}
static void     do_chip_info(void)
{

    ESP_LOGI(TAG, "%s started", __func__);

    /* Print chip information */
    esp_chip_info_t chip_info;
    uint32_t flash_size;
    esp_chip_info(&chip_info);
    printf("This is %s chip with %d CPU core(s), %s%s%s%s, ",
           CONFIG_IDF_TARGET,
           chip_info.cores,
           (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi/" : "",
           (chip_info.features & CHIP_FEATURE_BT) ? "BT" : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? "BLE" : "",
           (chip_info.features & CHIP_FEATURE_IEEE802154) ? ", 802.15.4 (Zigbee/Thread)" : "");

    unsigned major_rev = chip_info.revision / 100;
    unsigned minor_rev = chip_info.revision % 100;
    printf("silicon revision v%d.%d, ", major_rev, minor_rev);
    if(esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
        printf("Get flash size failed");
        return;
    }

    printf("%" PRIu32 "MB %s flash\n", flash_size / (uint32_t)(1024 * 1024),
           (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    printf("Minimum free heap size: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());

}

// Our device look-up - post I2C bus scan
static void FindAndInitDevices(void) 
{
    if(IsADS1115Present())
    {
        ESP_LOGI(TAG, "ADS1115 found - do the initialization");
        InitADS1115();
    }
    else
    {
        ESP_LOGE(TAG, "ADS1115 not found! Cannot acquire ECG signal data!");
    }
    
    if(IsDS3231Present())
    {
        ESP_LOGI(TAG, "DS3231 found - do the initialization");
    }
    else
    {
        ESP_LOGE(TAG, "DS3231 not found! - RTC needed for timestamping!");
    }
}

// AI suggested.


// Helper: Convert BCD format from DS3231 to regular integers
static uint8_t bcd_to_dec(uint8_t val) {
    return ((val / 16) * 10) + (val % 16);
}

/**
 * @brief ADC related items, AI suggested
 * 
 */

static void IRAM_ATTR ads_gpio_isr_handler(void* arg) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(ads_rdy_semaphore, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

// This is the AI recommendation: some slight changes in order to implement the code here.
// CORE 0: High-Priority ECG Acquisition
void adc_acquisition_task(void *pvParameters) 
{
    uint8_t tracking_channel = 0;
    adc_sample_t current_batch = {0};
    uint16_t raw_reading = 0;
    esp_err_t   retv;

    //i2c_dev_t   ADS1115;

    //ads1115_write_reg(ADS1115_CFG_REG, ads_mux_config[tracking_channel]);
    retv = ads111x_write_reg( &ECGDev_ads1115, REG_CONFIG, ads_mux_config[tracking_channel]);
    if(retv != ESP_OK)
    {
        ESP_LOGE(TAG, "Error from ads111x_write_reg = %d(%s)", retv, esp_err_to_name(retv));
    }

    while (1) {
        if (xSemaphoreTake(ads_rdy_semaphore, portMAX_DELAY) == pdTRUE) {
            
            //if (ads1115_read_reg(ADS1115_CONV_REG, &raw_reading) == ESP_OK) {
            if (ads111x_read_reg(&ECGDev_ads1115, REG_CONFIG, &raw_reading) == ESP_OK) {
                current_batch.channels[tracking_channel] = (int16_t)raw_reading;
            }

            uint8_t next_channel = (tracking_channel + 1) % 4;

            if (tracking_channel == 3) {
                // High-precision microsecond hardware timer stamp for ECG baseline alignment
                current_batch.micro_timestamp = esp_timer_get_time();
                xQueueSend(adc_queue, &current_batch, 0);
            }

            tracking_channel = next_channel;
            //ads1115_write_reg(ADS1115_CFG_REG, ads_mux_config[tracking_channel]);
            retv = ads111x_write_reg(&ECGDev_ads1115, REG_CONFIG, ads_mux_config[tracking_channel]);
            if (retv != ESP_OK)
            {
                ESP_LOGE(TAG, "Error from ads111x_write_reg = %d(%s)", retv, esp_err_to_name(retv));
            }
        }
    }
}

/**
 * @brief AI recommended functionality for handling the MicroSD file access.
 * 
 */
// CORE 1: Low-Priority RTC Syncing & SD Card Storage 
void sd_writer_task(void *pvParameters) {
    adc_sample_t log_data;
    TickType_t last_rtc_check = xTaskGetTickCount();
    rtc_time_t local_clock_copy;

    // Seed the initial wall clock
    ds3231_read_time(&local_clock_copy);
    global_wall_clock = local_clock_copy;

    while (1) {
        // Safe RTC Polling: Only check the physical chip once every 1000ms
        if ((xTaskGetTickCount() - last_rtc_check) > pdMS_TO_TICKS(1000)) {
            if (ds3231_read_time(&local_clock_copy) == ESP_OK) {
                global_wall_clock = local_clock_copy;
            }
            last_rtc_check = xTaskGetTickCount();
        }

        // Pull incoming high-speed ECG data from queue
        if (xQueueReceive(adc_queue, &log_data, portMAX_DELAY) == pdTRUE) {
            FILE* f = fopen("/sdcard/ecg_trace.csv", "a");
            if (f != NULL) {
                // Formats as: HH:MM:SS,Microseconds,Ch0,Ch1,Ch2,Ch3
                fprintf(f, "%02d:%02d:%02d,%llu,%d,%d,%d,%d\n", 
                        global_wall_clock.hour,
                        global_wall_clock.minute,
                        global_wall_clock.second,
                        log_data.micro_timestamp,
                        log_data.channels[0], 
                        log_data.channels[1], 
                        log_data.channels[2], 
                        log_data.channels[3]);
                fclose(f);
            }
        }
    }
}

/**
 * @brief Setup_ADC_SD: kick off the ADC and SD tasks.
 * 
 */
void Setup_ADC_SD(void)
{
    // The following items were all suggestions from Google AI (see docs/AI-assisted-circuit-and-SW-design)
    // We are going to setup the ADS1115 to generate the ALERT/RDY signal when an ADC capture is ready to 
    // be acquired. This eliminates polling by letting the chip generate an interrupt when a sample is ready
    // to be retrieved.
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_NEGEDGE,
        .pin_bit_mask = (1ULL << ADS_ALERT_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE
    };
    gpio_config(&io_conf);

    // We use a semaphore system to notify the ADC acquisition task an process the ADC sample (which is 
    // "consumed" by the ECG processing). The sample is passed to the pask via a message queue.
    ads_rdy_semaphore = xSemaphoreCreateBinary();
    adc_queue = xQueueCreate(100, sizeof(adc_sample_t)); // Expanded queue size for dense trace data

    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_isr_handler_add(ADS_ALERT_GPIO, ads_gpio_isr_handler, NULL);

    // Pin high priority capture to Core 0, file management to Core 1
    xTaskCreatePinnedToCore(adc_acquisition_task, "ECG_Acq", 4096, NULL, 10, NULL, 0);
    xTaskCreatePinnedToCore(sd_writer_task, "SD_Write", 4096, NULL, 4, NULL, 1);
}

void app_main(void)
{
    ESP_LOGI(TAG, "%s: starting", __func__);

    do_chip_info();
    xMutex = xSemaphoreCreateMutex();
    if(xMutex == NULL)
    {
        ESP_LOGE(TAG, "xSemaphoreCreateMutex = NULL returned!");
        do_restart();
    }

    InitI2C();

    // check for device presence, and do the necessary installation.
    FindAndInitDevices();

    // Setup the foundation for the ADC and the MicroSD writer
    Setup_ADC_SD();


    do_restart();
    
}
