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

static const char *TAG = "main";

extern void     i2c_master_init(void);
extern void     InitI2C(void);
extern bool     IsADS1115Present(void);
extern bool     IsDS3231Present(void);

// if we decide to use HTTP, we might need to use this to avoid I2C timeouts
SemaphoreHandle_t	xMutex;

i2c_master_bus_handle_t bus_handle;
i2c_master_dev_handle_t dev_handle;

typedef struct {
    uint64_t micro_timestamp; // Microsecond accuracy for ECG spacing
    int16_t channels[4];
} adc_sample_t;

typedef struct {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
} rtc_time_t;

/**
 * @brief ADC related items, AI suggested
 * 
 */


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

extern esp_err_t	InitADS1115(void);

static void FindAndInitDevices(void) 
{
    if(IsADS1115Present())
    {
        ESP_LOGI(TAG, "ADS1115 found - do the initialization");
        InitADS1115();
    }
    else
    {
        ESP_LOGI(TAG, "ADS1115 not found! Cannot acquire ECG signal data!");
    }
    
    if(IsDS3231Present())
    {
        ESP_LOGI(TAG, "DS3231 found - do the initialization");
    }
    else
    {
        ESP_LOGI(TAG, "DS3231 not found! - RTC needed for timestamping!");
    }
}

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


    do_restart();
    
}
