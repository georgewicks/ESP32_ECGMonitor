/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include <time.h>
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
#include "driver/spi_common.h"      // for SPI mode
#include "pcf8574.h"
#include "nvs_flash.h"

// For Time sync with NTP:
#include "esp_netif_sntp.h"
#include "esp_sntp.h"
#include <esp_log.h>
#include "spoof_i2cdev.h"
#include "common.h"
#include "ADCCapture.h"

// Define GPIO mappings
#define PIN_NUM_MISO 13
#define PIN_NUM_MOSI 11
#define PIN_NUM_CLK  12
#define PIN_NUM_CS   10

#define MOUNT_POINT "/sdcard"

#define TEST_ECG_CSV
#define PKG_SAMPLES_QRS


static const char *TAG = "main";

extern void     basic_test_loop( void );

extern void     i2c_master_init(void);
extern void     InitI2C(void);
extern bool     IsADS1115Present(void);
extern bool     IsDS3231Present(void);

// if we decide to use HTTP, we might need to use this to avoid I2C timeouts
SemaphoreHandle_t	xMutex;

i2c_master_bus_handle_t bus_handle;
i2c_master_dev_handle_t dev_handle;

extern esp_err_t    ads111x_read_reg( i2c_dev_t *dev, uint8_t reg, uint16_t *val);
extern esp_err_t    ads111x_write_reg(i2c_dev_t *dev, uint8_t reg, uint16_t val);
extern i2c_dev_t	ECGDev_ads1115;
extern esp_err_t	InitADS1115(void);
extern i2c_dev_t    DS3231_dev;
extern esp_err_t    InitDS3231(void);
extern void         wifi_init_sta(void);
extern void         fetch_sntp_time(void);

// ECG sample handling: we could keep the same 4-channel sample array,
// sending to the sd_task, that we eventually turn into a full 1 second 
// trace buffer for the ECG processing.
extern void         PushToECGProc(ECG_adc_sample_t *curECGptr);

#define REC_PING    0
#define REC_PONG    1
ECG_adc_sample_t    ECGPing;
ECG_adc_sample_t    ECGPong;

ECG_adc_sample_t    *curECGptr = &ECGPing;
int     ECG_Buff_indicator = REC_PING;

#define ECG_REC_MAX     (215)



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
// static volatile rtc_time_t global_wall_clock = {0};
static volatile struct tm  global_wall_clock = {0};

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

/* -------------------------------------------------------------------------------------------------------------- */

/**
 * @brief For handling the MicroSD card adapter in SPI mode.
 * 
 */

void    InitSD_SPI(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "%s: called", __func__);

    // 1. Setup VFS and SD Card configuration
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };
    sdmmc_card_t *card;

    // 2. Initialize the SPI Bus
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    
    // Using SPI2_HOST (commonly mapped as FSPI)
    ret = spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        printf("Failed to initialize SPI bus.\n");
        return;
    }
    // 3. Configure the slot device (SPI mode configuration)
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI2_HOST; // Must match the initialized SPI host
    ESP_LOGI(TAG, "%s:  spi_bus_initialize", __func__);

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = SPI2_HOST;

    // 4. Mount the filesystem
    printf("Mounting filesystem...\n");
    ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            printf("Failed to mount filesystem. If card is unformatted, set format_if_mount_failed.\n");
        } else {
            printf("Failed to initialize the card (%s). Check wiring.\n", esp_err_to_name(ret));
        }
        return;
    }

    // Card is successfully mounted. Read/write via standard POSIX functions
    printf("Filesystem mounted. Card info: Name: %s\n", card->cid.name);
    
    // Example Write
    FILE *f = fopen(MOUNT_POINT "/log.txt", "w");
    if (f == NULL) {
        printf("Failed to open file for writing\n");
        return;
    }
    fprintf(f, "Hello from ESP32-S3! Data logged successfully.\n");
    fclose(f);
    
    printf("File written successfully.\n");
}



// ******************************************************************

// Our device look-up - post I2C bus scan
static void FindAndInitDevices(void) 
{
    ESP_LOGI(TAG, "%s: called", __func__);
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
        InitDS3231();
    }
    else
    {
        ESP_LOGE(TAG, "DS3231 not found! - RTC needed for timestamping!");
    }
}

/* --------------------------------------------------------------------------------------------- */


/**
 * @brief ADC related items, AI suggested
 * 
 */


// Helper: Convert BCD format from DS3231 to regular integers
static uint8_t bcd_to_dec(uint8_t val) {
    return ((val / 16) * 10) + (val % 16);
}
static void IRAM_ATTR ads_gpio_isr_handler(void* arg) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(ads_rdy_semaphore, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

volatile  uint32_t  adc_signal_count = 0;                   // count of ADC semaphore signals - ADC ISR gives the semaphore

#ifdef AI_DEBUG

// AI suggested diagnostics
void    Check_ADS1115_Regs( void )
{
    // 2026-08-31: ESP Docs AI Assistant recommendations:

    // Add this readback immediately after the write:
    uint16_t config_val = 0;
    ads111x_read_reg(&ECGDev_ads1115, REG_CONFIG, &config_val);
    ESP_LOGI(TAG, "Config AFTER write = 0x%04X (expected 0xC4E8)", config_val);

    // Also verify threshold registers:
    uint16_t lo = 0, hi = 0;
    ads111x_read_reg(&ECGDev_ads1115, REG_THRESH_L, &lo);
    ads111x_read_reg(&ECGDev_ads1115, REG_THRESH_H, &hi);
    ESP_LOGI(TAG, "LO_THRESH=0x%04X (need 0x0000), HI_THRESH=0x%04X (need 0x8000)", lo, hi);
}

void    Check_ADS1115_Regs_2(void)
{
    // Temporarily add this to adc_acquisition_task to verify the ADS1115
    // is responding before relying on the interrupt:
    uint16_t config_val = 0;
    ads111x_read_reg(&ECGDev_ads1115, REG_CONFIG, &config_val);
    ESP_LOGI(TAG, "ADS1115 config register = 0x%04X", config_val);
    // Bit 15 (OS) = 1 means conversion complete
    // If bit 15 is always 0, the chip is not converting
}

#endif

// This is the AI recommendation: some slight changes in order to implement the code here.
// CORE 0: High-Priority ECG Acquisition
void adc_acquisition_task(void *pvParameters) 
{
    uint8_t tracking_channel = 0;
    adc_sample_t current_batch = {0};
    uint16_t raw_reading = 0;
    esp_err_t   retv;

    // ECG sample handling
    curECGptr = &ECGPing;       // Kick off with the Ping Buffer
    int ESampCnt = 0;

    //i2c_dev_t   ADS1115;
    ESP_LOGI(TAG, "%s: called", __func__);

    //ads1115_write_reg(ADS1115_CFG_REG, ads_mux_config[tracking_channel]);
    retv = ads111x_write_reg( &ECGDev_ads1115, REG_CONFIG, ads_mux_config[tracking_channel]);
    if(retv != ESP_OK)
    {
        ESP_LOGE(TAG, "Error from ads111x_write_reg = %d(%s)", retv, esp_err_to_name(retv));
        ESP_LOGE(TAG, " what was supposedly written to the config register = %X", ads_mux_config[tracking_channel]);
    }

#ifdef AI_DEBUG
    Check_ADS1115_Regs();
#endif

    while (1) {

#ifdef AI_DEBUG
    Check_ADS1115_Regs_2(void);
#endif
        if (xSemaphoreTake(ads_rdy_semaphore, portMAX_DELAY) == pdTRUE) 
        {
            adc_signal_count++;

            // if (ads1115_read_reg(ADS1115_CONV_REG, &raw_reading) == ESP_OK) {
            if (ads111x_read_reg(&ECGDev_ads1115, REG_CONVERSION, &raw_reading) == ESP_OK)
            {
                current_batch.channels[tracking_channel] = (int16_t)raw_reading;
            }

            uint8_t next_channel = (tracking_channel + 1) % 4;

            if (tracking_channel == 3) {
                // High-precision microsecond hardware timer stamp for ECG baseline alignment
                current_batch.micro_timestamp = esp_timer_get_time();
                xQueueSend(adc_queue, &current_batch, 0);
            }

            tracking_channel = next_channel;
    
            retv = ads111x_write_reg(&ECGDev_ads1115, REG_CONFIG, ads_mux_config[tracking_channel]);
            if (retv != ESP_OK)
            {
                ESP_LOGE(TAG, "Error from ads111x_write_reg = %d(%s)", retv, esp_err_to_name(retv));
            }
        }
    }
}

/**
 * @brief   Google AI supplied Time Sync code
 * 
 */

#if 0
// Relevant include files already put in..

#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "esp_netif_sntp.h"
#include "esp_sntp.h"
#include "esp_log.h"
#include "ds3231.h" // Assuming an external component like esp-idf-ds3231

static const char *TAG = "time_sync";
#endif

#if 0
// Initialize SNTP to fetch internet time
void initialize_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    if (esp_sntp_enabled())
    {
        esp_sntp_stop(); // Safely shuts down lwIP SNTP client
    }

    // Now safe to reconfigure without crashing
    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "new.pool.ntp.org");
    esp_sntp_init();
}
#endif

// Main sync routine
void sync_ntp_to_ds3231(i2c_dev_t *dev)
{
    void do_sntp_app_main(void);

    // Wait until system time is updated by NTP (retry up to 30 seconds)
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 30;

    while (timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to sync... (%d/%d)", retry, retry_count);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
    }

    if (retry == retry_count) {
        ESP_LOGE(TAG, "System time sync with NTP failed!");
        return;
    }

    ESP_LOGI(TAG, "NTP Synchronized successfully. Updating DS3231 RTC...");

    // Write current system time struct to DS3231
    if (ds3231_set_time(dev, &timeinfo) == ESP_OK) {
        ESP_LOGI(TAG, "DS3231 successfully updated with Wi-Fi time.");
    } else {
        ESP_LOGE(TAG, "Failed to write time to DS3231.");
    }
}

volatile uint32_t   sd_signal_count = 0;        // count of Queue recvs - generated from the ADC task
void    AppendToECGPacket(adc_sample_t *log_data)
{
    static  uint32_t    ECG_4Channel_count = 0;

    if(ECG_4Channel_count >= ECG_REC_MAX)
    {
        ESP_LOGE(TAG, "Out of bounds for ECG_4Channel_count! value = %d", ECG_4Channel_count);
    }

    if(ECG_4Channel_count < ECG_REC_MAX)
    {
        curECGptr->samples[0][ECG_4Channel_count] = log_data->channels[0];
        curECGptr->samples[1][ECG_4Channel_count] = log_data->channels[1];
        curECGptr->samples[2][ECG_4Channel_count] = log_data->channels[2];
        curECGptr->samples[3][ECG_4Channel_count] = log_data->channels[3];
        ECG_4Channel_count++;
        if(ECG_4Channel_count == ECG_REC_MAX)
        {
            PushToECGProc(curECGptr);       // Done, now pass it to the ECG processing

            // Switch working ECG ADC buffers to avoid stomping on working data to capture
            if(ECG_4Channel_count == REC_PING)
            {
                curECGptr = &ECGPong;
                ECG_Buff_indicator = REC_PONG;
            }    
            else
            {
                curECGptr = &ECGPing;
                ECG_Buff_indicator = REC_PING;
            }
            ECG_4Channel_count = 0;         // ready new record...
        }
    }
}

/**
 * @brief AI recommended functionality for handling the MicroSD file access.
 * 
 */
// CORE 1: Low-Priority RTC Syncing & SD Card Storage 
void sd_writer_task(void *pvParameters) {
    adc_sample_t    log_data;
    TickType_t      last_rtc_check = xTaskGetTickCount();
    // rtc_time_t      local_clock_copy;
    struct tm       local_clock_copy;
    esp_err_t       retv;
    ESP_LOGI(TAG, "%s: called", __func__);

    // Seed the initial wall clock
    //ds3231_read_time(&local_clock_copy);
    retv = ds3231_get_time( &DS3231_dev, &local_clock_copy);
    global_wall_clock = local_clock_copy;

    while (1) {
        // Safe RTC Polling: Only check the physical chip once every 1000ms
        if ((xTaskGetTickCount() - last_rtc_check) > pdMS_TO_TICKS(1000)) {
            retv = ds3231_get_time(&DS3231_dev, &local_clock_copy);
            if (retv == ESP_OK) {
                global_wall_clock = local_clock_copy;
            }
            else
            {
                ESP_LOGE(TAG,"ds3232_get_time error = %d(%s)", retv, esp_err_to_name(retv));
            }
            last_rtc_check = xTaskGetTickCount();
        }

        // Pull incoming high-speed ECG data from queue
        if (xQueueReceive(adc_queue, &log_data, portMAX_DELAY) == pdTRUE) {
            sd_signal_count++;
#ifdef  TEST_ECG_CSV
            FILE* f = fopen("/sdcard/ecg_trace.csv", "a");
            if (f != NULL) {
                // Formats as: HH:MM:SS,Microseconds,Ch0,Ch1,Ch2,Ch3
                fprintf(f, "%02d:%02d:%02d,%llu,%d,%d,%d,%d\n", 
                        global_wall_clock.tm_hour,
                        global_wall_clock.tm_min,
                        global_wall_clock.tm_sec,
                        log_data.micro_timestamp,
                        log_data.channels[0], 
                        log_data.channels[1], 
                        log_data.channels[2], 
                        log_data.channels[3]);
                fclose(f);
            }
#endif
#ifdef  PKG_SAMPLES_QRS
            AppendToECGPacket(&log_data);
#endif
        }
    }
}

/**
 * @brief Setup_ADC_SD: kick off the ADC and SD tasks.
 * 
 */
void Setup_ADC_SD(void)
{
    esp_err_t   retv;

    ESP_LOGI(TAG, "%s: called", __func__);
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

    retv = gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    if(retv != ESP_OK)
    {
        ESP_LOGD(TAG, "error from gpio_install_isr_service = %d(%s)", retv, esp_err_to_name(retv));
    }
    else
    {
        ESP_LOGI(TAG, "gpio_install_isr_service OK");
    }

    retv = gpio_isr_handler_add(ADS_ALERT_GPIO, ads_gpio_isr_handler, NULL);
    if(retv != ESP_OK)
    {
        ESP_LOGD(TAG, "error from gpio_isr_handler_add = %d(%s)", retv, esp_err_to_name(retv));
    } else {
        ESP_LOGI(TAG, "gpio_isr_handler_add OK on GPIO %d", ADS_ALERT_GPIO);
    }

    // Pin high priority capture to Core 0, file management to Core 1
    xTaskCreatePinnedToCore(adc_acquisition_task, "ECG_Acq", 8192, NULL, 10, NULL, 0);
    xTaskCreatePinnedToCore(sd_writer_task, "SD_Write", 4096, NULL, 4, NULL, 1);
}

void app_main(void)
{
    int sec_count = 0;

    ESP_LOGI(TAG, "%s: starting", __func__);

    do_chip_info();
    xMutex = xSemaphoreCreateMutex();
    if(xMutex == NULL)
    {
        ESP_LOGE(TAG, "xSemaphoreCreateMutex = NULL returned!");
        do_restart();
    }

    InitI2C();
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    if (CONFIG_LOG_MAXIMUM_LEVEL > CONFIG_LOG_DEFAULT_LEVEL) {
        /* If you only want to open more logs in the wifi module, you need to make the max level greater than the default level,
         * and call esp_log_level_set() before esp_wifi_init() to improve the log level of the wifi module. */
        esp_log_level_set("wifi", CONFIG_LOG_MAXIMUM_LEVEL);
    }

    // check for device presence, and do the necessary installation.
    FindAndInitDevices();

    // Instantiate the PCF8574 device (Default addr is usually 0x20 or 0x3F)
    ESP_LOGI(TAG, "Init the PCF8574");
    pcf8574_dev_t pcf_dev;
    ESP_ERROR_CHECK(pcf8574_init(bus_handle, 0x20, &pcf_dev));
    
    // Initialize WiFi - we need WiFi enabled in order to do NTP synchronization.
    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();

    fetch_sntp_time();              // ← ADD THIS - starts SNTP and blocks until synced
   
    // Now, we can sync up the RTC...
    sync_ntp_to_ds3231(&DS3231_dev);

    //Init_sdmmc();
    InitSD_SPI();

    // Setup the foundation for the ADC and the MicroSD writer
    Setup_ADC_SD();

#if  0
// Pause until we get the 16-DIP package of the PCF8574 (GPIO Extender)
        ESP_LOGI(TAG, "about to go blinking...");
 
    // 3. Test writing (e.g., blink an LED on pin 0)
    while(1) {
        ESP_LOGI(TAG, "command to turn on LED.. ");
        ret = pcf8574_write(&pcf_dev, 0x01);
        if(ret == ESP_OK)
        {
            ESP_LOGI(TAG, "LED should be on");
        }
        else
        {
            ESP_LOGE(TAG, "err from pcf8574_write = %d", ret);
        }
        vTaskDelay(1000/portTICK_PERIOD_MS);
        pcf8574_write(&pcf_dev, 0x00);
        vTaskDelay(1000/portTICK_PERIOD_MS);
        ESP_LOGI(TAG, "blinking...");
    }
#endif
    int count = 0;
    while(1)
    {
        // This is a visual way of confirming that the ADC capture is functioning... or not.
        // TODO: Move this to a basic POST test, that can be invoked for testing.
        // ESP_LOGI(TAG, "Running... SD count = %d, ADC count = %d",sd_signal_count,adc_signal_count);
        vTaskDelay(1000/portTICK_PERIOD_MS);
        sec_count++;
        if(sec_count == 60)
        {
            break;
        }
    }

    do_restart();
    
}

