/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include <time.h>
#include <math.h>
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
#include "Logging.h"
#include "ISHNE.h"
#include "MPU6050.h"

// For the configuration file on the AT24C32, located on the RTC board
#include "i2c_eeprom.h"

#define MOUNT_POINT "/sdcard"

#define TEST_ECG_CSV
#define TEST_ECG_ISHNE
#define PKG_SAMPLES_QRS

/* Interactive SD debug: after each init attempt, pause and let the operator
 * press Y (continue boot) or N (re-run the SD init). Helps chase flaky
 * breadboard wiring without reflashing between fixes. */
#define SD_DEBUG_INTERACTIVE   (1)
#define SD_DEBUG_CLOCK_KHZ     (400)   /* slow SPI while probing marginal wiring */
#define SD_RETRY_DELAY_MS      (2000)  /* delay between auto-retry probes */

/* CSV writer retry: survive transient SD-card write glitches. */
#define CSV_WRITE_RETRIES      (3)
#define CSV_RETRY_DELAY_MS     (5)

/* ---------------------------------------------------------------------------
 * ISHNE (Holter binary) output configuration.
 *
 * NOTE on amplitude resolution: the ISHNE "resolution" field is a signed
 * 16-bit integer of nanovolts per LSB, so it can hold at most 32767 nV/LSB.
 * The ADS1115 at PGA = +/-2.048 V has a true resolution of 62.5 uV =
 * 62500 nV/LSB, which overflows. We therefore store the raw ADC counts with
 * resolution 31250 nV/LSB (the +/-1.024 V value): the file is structurally
 * valid and timing/shape are exact, but amplitude reads 0.5x too small.
 * Proper fix: differential mode at max PGA (+/-0.256 V -> 7812 nV/LSB fits),
 * which also removes the AD8232's ~1.5 V DC offset.
 * ------------------------------------------------------------------------- */
#define ISHNE_SAMPLE_RATE_HZ   (350)   /* nominal per-channel rate (actual ~353) */
#define ISHNE_RESOLUTION_NV    (31250) /* see NOTE above: true value is 62500 */
#define ISHNE_NLEADS           (1)
/* Which ADC channel each ISHNE lead reads from, and the lead code. Only the
 * AD8232 (ch2 = ADS1115 B AIN0) is connected today; verify the limb-lead code
 * against the actual electrode placement (RA/LA/LL -> Lead I by default). */
static const int16_t ishne_lead_spec[ISHNE_NLEADS]    = { ISHNE_LEAD_I };
static const int     ishne_lead_channel[ISHNE_NLEADS] = { 2 };

/* Self-recording test harness: after boot, count down TEST_SETTLE_SECONDS
 * (operator leans back and stays still), record for TEST_RECORD_SECONDS,
 * then close the CSV and park the task forever so the operator can move
 * without contaminating the trace. */
#define TEST_SETTLE_SECONDS     (10)
#define TEST_RECORD_SECONDS     (120)

/* Leads-off / signal-invalid detection: the AD8232 output pegs at the ADC
 * rail when an electrode loses contact (the LO+/LO- pins only cover IN+/IN-,
 * not the driven RLD electrode, so the data itself is the reliable tell). */
#define ECG_SIGNAL_CHANNEL      (2)     /* ADS1115 #2 AIN0 = AD8232 OUTPUT   */
#define ECG_RAIL_MARGIN         (100)   /* LSB within full-scale => railed   */
#define ECG_RAIL_SAMPLES        (70)    /* ~200 ms at ~357 SPS before warn   */

/* Motion-artifact gating (MPU6050 accelerometer).
 * |accel magnitude - 1g| is the "dynamic" acceleration: body motion raises it
 * above the resting ~1g. When it stays above the threshold long enough we flag
 * the segment as artifact so QRS/RR processing ignores it (raw ECG is still
 * recorded). */
#define MOTION_THRESHOLD_G        (0.20f)   /* dynamic g above which = motion  */
#define MOTION_ON_COUNT           (5)       /* consecutive reads to assert      */
#define MOTION_OFF_COUNT          (20)      /* consecutive reads to clear       */
#define MOTION_SAMPLE_INTERVAL_MS (10)      /* ~100 Hz motion sampling          */

static bool bMPU6050 = false;
static volatile bool motion_artifact_active = false;

/**
 * @brief Component and System testing
 * 
 */
#define SYSTEST             (true)
#define ADS1115_TEST        (true)
#define DS3231_TEST         (true)
#define PCF8574_TEST        (false)
#define SD_TEST             (true)
bool    bTestSysComponents = SYSTEST;
bool    bEnableADS1115 = ADS1115_TEST;
bool    bEnableDS3231 = DS3231_TEST;
bool    bEnableF8574 = PCF8574_TEST;
bool    bEnableSD = SD_TEST;

static const char *TAG = "main-ECGMon";

extern void     basic_test_loop( void );
extern void     i2c_master_init(void);
extern void     InitI2C(void);
extern bool     IsADS1115Present(void);
extern bool     IsADS1115BPresent(void);
extern bool     IsDS3231Present(void);
extern bool     IsMPU6050Present(void);

extern bool     bWifi_Connected; // Global flag to indicate Wi-Fi connection status

SemaphoreHandle_t	xMutex;

i2c_master_bus_handle_t bus_handle;

// Device handles - one per device
extern i2c_master_dev_handle_t ads1115_handle;  // 0x48
extern i2c_master_dev_handle_t ads1115_b_handle; // 0x49
extern i2c_master_dev_handle_t gpio_exp_handle; // 0x27  PCF8574
extern i2c_master_dev_handle_t eeprom_handle;   // 0x57
extern i2c_master_dev_handle_t rtc_handle;      // 0x68
extern i2c_master_dev_handle_t mpu6050_handle;  // 0x69

#ifdef  OLDER
i2c_master_dev_handle_t dev_handle;
#endif

extern esp_err_t    ads111x_read_reg( i2c_dev_t *dev, uint8_t reg, uint16_t *val);
extern esp_err_t    ads111x_write_reg(i2c_dev_t *dev, uint8_t reg, uint16_t val);
extern i2c_dev_t	ECGDev_ads1115;
extern i2c_dev_t	ECGDev_ads1115_b;
extern esp_err_t	InitADS1115(void);
extern esp_err_t	InitADS1115B(void);
extern i2c_dev_t    DS3231_dev;
extern esp_err_t    InitDS3231(void);
extern void         wifi_init_sta(void);
extern void         fetch_sntp_time(void);
extern void         wifi_shutdown(void);

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
static SemaphoreHandle_t ads_rdy_semA = NULL;
static SemaphoreHandle_t ads_rdy_semB = NULL;

// Per-chip acquisition-task context
typedef struct {
    i2c_dev_t        *dev;
    SemaphoreHandle_t rdy_sem;
    uint8_t           chip;        // 0 -> channels[0..1], 1 -> channels[2..3]
    int               alert_gpio;  // for diagnostics
} ads_chip_ctx_t;

// One 2-channel pair published by each chip task
typedef struct {
    uint8_t  chip;
    uint64_t micro_timestamp;
    int16_t  values[2];
} ads_pair_t;

// Each chip muxes 2 channels (AIN0/GND, AIN1/GND): single-shot, 860 SPS,
// active-low, COMP_QUE=00 (RDY pin enabled). See DEEPSEEK_FIX_ADC_IRQ.md.
static const uint16_t ads_mux_config_pair[2] = {
    0xC5E0, // AIN0/GND
    0xD5E0  // AIN1/GND
};

// Global wall clock shared safely via thread updates
// static volatile rtc_time_t global_wall_clock = {0};
static volatile struct tm  global_wall_clock = {0};


/* ------------------------------------------------------------------------ */

static void     do_restart(void)
{
    /* Fatal-error recovery path (e.g. mutex creation failed): restart
     * immediately rather than counting down — this isn't a graceful shutdown. */
    ESP_LOGE(TAG, "%s: fatal error — restarting now", __func__);
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
 * @brief Configuration File Handling. We will use the AT24C32 on the RTC board. See the README.md for info  
 * 
 */

#define CFG_LENGTH      (1024)

void    Init_EEPROM(void)
{
    ESP_LOGI(TAG, "%s called", __func__);

    i2c_eeprom_config_t eeprom_config = {
        .eeprom_device.scl_speed_hz = 100000,
        .eeprom_device.device_address = 0x57, // matches your scan
        .addr_wordlen = 2,                    // AT24C32 uses 16-bit addressing
        //.page_size = 32,                      // AT24C32 page size
        .write_time_ms = 10,                  // typical AT24C32 write cycle
    };

    i2c_eeprom_handle_t eeprom_handle;
    ESP_ERROR_CHECK(i2c_eeprom_init(bus_handle, &eeprom_config, &eeprom_handle));

 
    uint32_t block_addr = 0x0000;
    uint8_t buf[CFG_LENGTH];
    for (int i = 0; i < CFG_LENGTH; i++) {
        buf[i] = i;
    }
    uint8_t read_buf[CFG_LENGTH];

    ESP_ERROR_CHECK(i2c_eeprom_read(eeprom_handle, block_addr, read_buf, CFG_LENGTH));

    ESP_LOGI(TAG," CFG contents: ");
    for (int i = 0; i < 4; i++) {
        printf("%02X",read_buf[i]);
    }
    printf("\n");
    ESP_LOGI(TAG, "... done");

  
}


/* -------------------------------------------------------------------------------------------------------------- */

/**
 * @brief Interactive SD debug: probe the card, then let the operator decide
 * whether to continue booting or re-run the probe after fixing the wiring.
 *
 * Runs at a slow SPI clock (SD_DEBUG_CLOCK_KHZ) because flaky breadboard
 * wiring often works at low speed and fails at the default 20 MHz. The SPI
 * bus is initialized once (it cannot be re-initialized); only the card probe
 * is repeated.
 */
void    InitSD_SPI_debug(void)
{
    ESP_LOGI(TAG, "%s: interactive mode (Y=continue, N=re-init)", __func__);

    if(bEnableSD == false)
    {
        ESP_LOGI(TAG, "MicroSD is not enabled! -- exiting");
        return;
    }

    // 1. SPI bus init: once only.
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    if (spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO) != ESP_OK) {
        printf("Failed to initialize SPI bus.\n");
        return;
    }

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI2_HOST;
    host.max_freq_khz = SD_DEBUG_CLOCK_KHZ;

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = SPI2_HOST;

    sdmmc_card_t *card = NULL;

    for (int attempt = 1; ; attempt++) {
        printf("\n--- SD init attempt #%d ---\n", attempt);
        card = NULL;
        esp_err_t ret = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host, &slot_config,
                                                &mount_config, &card);
        if (ret == ESP_OK) {
            printf("MOUNT OK. Card info: Name: %s\n", card->cid.name);
        } else if (ret == ESP_FAIL) {
            printf("MOUNT FAILED (ESP_FAIL): filesystem mount failed.\n");
        } else {
            printf("MOUNT FAILED (0x%x = %s). Check wiring.\n", ret, esp_err_to_name(ret));
        }

        printf(">>> Press Y to continue booting, or N to re-run SD init: ");
        fflush(stdout);

        char line[8] = {0};
        if (fgets(line, sizeof(line), stdin) == NULL) {
            /* No console input available (USB unplugged, or a capture-only
             * monitor): auto-retry so reseating a wire recovers on its own
             * without requiring a keypress. */
            printf("(no console input) auto-retrying in %d s...\n", SD_RETRY_DELAY_MS / 1000);
            vTaskDelay(pdMS_TO_TICKS(SD_RETRY_DELAY_MS));
            continue;
        }

        if (line[0] == 'Y' || line[0] == 'y') {
            break;
        }

        /* 'N' (or anything else): clean up and re-probe. */
        if (ret == ESP_OK) {
            esp_vfs_fat_sdcard_unmount(MOUNT_POINT, card);
            card = NULL;
        }
        vTaskDelay(pdMS_TO_TICKS(SD_RETRY_DELAY_MS));
    }

    if (card != NULL) {
        // Mirror ESP_LOG output to the card now that it is mounted.
        if (logging_start(MOUNT_POINT "/debug.log") == NULL) {
            ESP_LOGE(TAG, "Failed to open debug.log for logging");
        } else {
            ESP_LOGI(TAG, "Log output mirrored to %s/debug.log", MOUNT_POINT);
        }

        FILE *f = fopen(MOUNT_POINT "/log.txt", "w");
        if (f != NULL) {
            fprintf(f, "Hello from ESP32-S3! Data logged successfully.\n");
            fclose(f);
            printf("File written successfully.\n");
        }
    } else {
        printf("Proceeding WITHOUT a mounted SD card (file writes will fail).\n");
    }
}

/**
 * @brief For handling the MicroSD card adapter in SPI mode.
 * 
 */

void    InitSD_SPI(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "%s: called", __func__);

    if(bEnableSD == false)
    {
        ESP_LOGI(TAG, "MicroSD is not enabled! -- exiting");
        return;
    }

    // 1. Setup VFS and SD Card configuration
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,
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

    // Mirror ESP_LOG output (ESP_LOGI/ESP_LOGW/ESP_LOGE/...) to a file on the
    // card so recordings can run without a USB serial connection. Early boot
    // logs stay on UART only; everything after this point is captured too.
    if (logging_start(MOUNT_POINT "/debug.log") == NULL) {
        ESP_LOGE(TAG, "Failed to open debug.log for logging");
    } else {
        ESP_LOGI(TAG, "Log output mirrored to %s/debug.log", MOUNT_POINT);
    }
    
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
        ESP_LOGI(TAG, "ADS1115 #1 (0x48) found - do the initialization");
        InitADS1115();
    }
    else
    {
        ESP_LOGE(TAG, "ADS1115 #1 (0x48) not found! Cannot acquire ECG signal data!");
    }

    if(IsADS1115BPresent())
    {
        ESP_LOGI(TAG, "ADS1115 #2 (0x49) found - do the initialization");
        InitADS1115B();
    }
    else
    {
        ESP_LOGE(TAG, "ADS1115 #2 (0x49) not found! Only 2 ECG channels will be captured.");
    }
    
    ESP_LOGI(TAG, "Check for DS3231 ");
    if(IsDS3231Present())
    {
        ESP_LOGI(TAG, "DS3231 found - do the initialization");
        InitDS3231();
        //Init_EEPROM();
    }
    else
    {
        ESP_LOGE(TAG, "DS3231 not found! - RTC needed for timestamping!");
    }

    ESP_LOGI(TAG, "Check for MPU6050 ");
    if(IsMPU6050Present())
    {
        if (mpu6050_init(mpu6050_handle) == ESP_OK) {
            bMPU6050 = true;
            ESP_LOGI(TAG, "MPU6050 (0x69) found and initialized - motion gating enabled");
        } else {
            ESP_LOGE(TAG, "MPU6050 (0x69) init failed - motion gating disabled");
        }
    }
    else
    {
        ESP_LOGI(TAG, "MPU6050 (0x69) not found - motion gating disabled");
    }
    ESP_LOGI(TAG, "end FindAndInitDevices");
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

static volatile uint32_t isr_count = 0;     // written in ISR, read in main context

static void IRAM_ATTR ads_gpio_isr_handler(void* arg) {
    isr_count++;

    SemaphoreHandle_t sem = (SemaphoreHandle_t)arg;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(sem, &xHigherPriorityTaskWoken);
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
    ESP_LOGI(TAG, "Config AFTER write = 0x%04X (expected 0xC5E0)", config_val);

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

// High-priority ECG acquisition: one task instance per ADS1115 chip.
// Each instance muxes 2 channels and publishes a 2-sample pair to the queue.
void adc_acquisition_task(void *pvParameters) 
{
    ads_chip_ctx_t *ctx = (ads_chip_ctx_t *)pvParameters;
    uint8_t  ch = 0;
    int16_t  pair[2] = {0};
    uint16_t raw_reading = 0;
    esp_err_t retv;

    if(bEnableADS1115 == false)
    {
        ESP_LOGI(TAG, "ADS1115 is not enabled! -- deleting/exiting task");
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "%s: chip %d on GPIO %d", __func__, ctx->chip, ctx->alert_gpio);

    // Prime the first conversion (AIN0).
    retv = ads111x_write_reg(ctx->dev, REG_CONFIG, ads_mux_config_pair[0]);
    if (retv != ESP_OK)
    {
        ESP_LOGE(TAG, "chip %d: initial config write failed = %d(%s)",
                 ctx->chip, retv, esp_err_to_name(retv));
    }

    while (1)
    {
        if (xSemaphoreTake(ctx->rdy_sem, pdMS_TO_TICKS(2000)) == pdTRUE)
        {
            adc_signal_count++;

            uint8_t next = ch ^ 1;

            // Start the next channel's conversion immediately, then read the
            // result while that conversion is in progress. This overlaps the
            // I2C read with the new conversion, minimizing the idle gap between
            // single-shot conversions. The conversion register still holds the
            // result of the conversion that just completed.
            retv = ads111x_write_reg(ctx->dev, REG_CONFIG, ads_mux_config_pair[next]);
            if (retv != ESP_OK)
            {
                ESP_LOGE(TAG, "chip %d: config write failed = %d(%s)",
                         ctx->chip, retv, esp_err_to_name(retv));
            }

            retv = ads111x_read_reg(ctx->dev, REG_CONVERSION, &raw_reading);
            if (retv == ESP_OK)
            {
                pair[ch] = (int16_t)raw_reading;
            }
            else
            {
                ESP_LOGE(TAG, "chip %d: bad conversion! %d(%s)",
                         ctx->chip, retv, esp_err_to_name(retv));
            }

            if (ch == 1)
            {
                ads_pair_t out = {
                    .chip = ctx->chip,
                    .micro_timestamp = esp_timer_get_time(),
                };
                out.values[0] = pair[0];
                out.values[1] = pair[1];
                xQueueSend(adc_queue, &out, 0);
            }

            ch = next;
        }
        else
        {
            // No ALERT/RDY interrupt within 2 seconds: re-prime a conversion so the
            // capture loop can recover if the ready pin ever stalls.
            ESP_LOGW(TAG, "chip %d (GPIO %d): timeout waiting for ALERT/RDY",
                     ctx->chip, ctx->alert_gpio);
            ads111x_write_reg(ctx->dev, REG_CONFIG, ads_mux_config_pair[ch]);
        }
    }
}

/**
 * @brief   Google AI supplied Time Sync code
 * 
 */


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
    adc_sample_t    record = {0};
    uint8_t         got_mask = 0;
    TickType_t      last_rtc_check = xTaskGetTickCount();
    TickType_t      last_flush = xTaskGetTickCount();
    TickType_t      record_start = 0;
    TickType_t      last_status = 0;
    int             rail_count = 0;
    bool            leads_off = false;
    struct tm       local_clock_copy;
    esp_err_t       retv;
#ifdef  TEST_ECG_CSV
    FILE*           csv = NULL;
    long            csv_records = 0;   /* successful fprintf writes        */
    long            csv_errors  = 0;   /* failed fprintf writes             */
    long            csv_start_size = 0;
#endif
#ifdef  TEST_ECG_ISHNE
    ishne_writer_t  ishne;
    bool            ishne_active = false;
#endif

    ESP_LOGI(TAG, "%s: called", __func__);
    if(bEnableSD == false)
    {
        ESP_LOGI(TAG, "MicroSD is not enabled! -- deleting/exiting task");
        vTaskDelete(NULL);
        return;
    }
 
    // Seed the initial wall clock
    if(bEnableDS3231) {
        retv = ds3231_get_time( &DS3231_dev, &local_clock_copy);
        global_wall_clock = local_clock_copy;
    }
    else
    {
        ESP_LOGI(TAG, "%s: ** DS3231 is not enabled", __func__);
    }

#ifdef  TEST_ECG_CSV
    // Open once and keep the stream open; open/close per record throttles the
    // writer and silently drops samples.
    csv = fopen("/sdcard/ecg_trace.csv", "a");
    if (csv == NULL) {
        ESP_LOGE(TAG, "Failed to open /sdcard/ecg_trace.csv for append");
    } else {
        setvbuf(csv, NULL, _IOFBF, 8192);
        fseek(csv, 0L, SEEK_END);
        csv_start_size = ftell(csv);
        ESP_LOGI(TAG, "CSV opened for append; existing size = %ld bytes", csv_start_size);
    }
#endif

    // Give the operator a clear signal of when capture begins so they can
    // settle and remain motionless for the duration of the recording.
    for (int c = TEST_SETTLE_SECONDS; c > 0; c--) {
        ESP_LOGI(TAG, "Recording starts in %d s...", c);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    ESP_LOGI(TAG, "*** RECORDING STARTED: remain still for %d s ***", TEST_RECORD_SECONDS);

#ifdef  TEST_ECG_ISHNE
    // One ISHNE file per recording, timestamped so consecutive boot/record
    // cycles don't clobber each other.
    {
        time_t now_t = time(NULL);
        struct tm tmv;
        localtime_r(&now_t, &tmv);

        char ecg_path[64];
        snprintf(ecg_path, sizeof(ecg_path),
                 MOUNT_POINT "/ecg_%04d%02d%02d_%02d%02d%02d.ecg",
                 tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
                 tmv.tm_hour, tmv.tm_min, tmv.tm_sec);

        ishne_header_t hdr;
        ishne_header_init(&hdr);
        strncpy(hdr.first_name, "George", sizeof(hdr.first_name) - 1);
        strncpy(hdr.last_name,  "Wicks",  sizeof(hdr.last_name)  - 1);
        strncpy(hdr.patient_id, "GW-0001", sizeof(hdr.patient_id) - 1);
        hdr.sex  = ISHNE_SEX_MALE;
        hdr.race = ISHNE_RACE_CAUCASIAN;
        hdr.birth_date[0] = ISHNE_LEAD_ABSENT;   /* unknown */
        hdr.birth_date[1] = ISHNE_LEAD_ABSENT;
        hdr.birth_date[2] = ISHNE_LEAD_ABSENT;
        hdr.record_date[0] = (int16_t)tmv.tm_mday;
        hdr.record_date[1] = (int16_t)(tmv.tm_mon + 1);
        hdr.record_date[2] = (int16_t)(tmv.tm_year + 1900);
        hdr.file_date[0]   = (int16_t)tmv.tm_mday;
        hdr.file_date[1]   = (int16_t)(tmv.tm_mon + 1);
        hdr.file_date[2]   = (int16_t)(tmv.tm_year + 1900);
        hdr.start_time[0]  = (int16_t)tmv.tm_hour;
        hdr.start_time[1]  = (int16_t)tmv.tm_min;
        hdr.start_time[2]  = (int16_t)tmv.tm_sec;
        hdr.nleads = ISHNE_NLEADS;
        for (int l = 0; l < ISHNE_NLEADS; l++) {
            hdr.lead_spec[l]  = ishne_lead_spec[l];
            hdr.lead_qual[l]  = 1;
            hdr.resolution[l] = ISHNE_RESOLUTION_NV;
        }
        hdr.pacemaker = 0;
        strncpy(hdr.recorder, "ESP32-ECGMonitor", sizeof(hdr.recorder) - 1);
        hdr.sample_rate = ISHNE_SAMPLE_RATE_HZ;

        if (ishne_open(&ishne, ecg_path, &hdr) == 0) {
            ishne_active = true;
            ESP_LOGI(TAG, "ISHNE recording -> %s (%d lead(s), %d Hz)",
                     ecg_path, ISHNE_NLEADS, ISHNE_SAMPLE_RATE_HZ);
        } else {
            ESP_LOGE(TAG, "Failed to open ISHNE file %s", ecg_path);
        }
    }
#endif

    record_start = xTaskGetTickCount();
    last_flush = record_start;
    last_status = record_start;

    while (1) {
        // Safe RTC Polling: Only check the physical chip once every 1000ms
        if ((xTaskGetTickCount() - last_rtc_check) > pdMS_TO_TICKS(1000)) {
            if (bEnableDS3231)
            {
                retv = ds3231_get_time(&DS3231_dev, &local_clock_copy);
                if (retv == ESP_OK)
                {
                    global_wall_clock = local_clock_copy;
                }
                else
                {
                    ESP_LOGE(TAG, "ds3232_get_time error = %d(%s)", retv, esp_err_to_name(retv));
                }
                last_rtc_check = xTaskGetTickCount();
            }
        }

        // Pull incoming 2-channel pairs from the chip tasks and merge them into
        // a full 4-channel record once both chips have contributed a pair.
        ads_pair_t p;
        if (xQueueReceive(adc_queue, &p, portMAX_DELAY) == pdTRUE) {
            uint8_t base = p.chip * 2;
            record.channels[base + 0] = p.values[0];
            record.channels[base + 1] = p.values[1];
            got_mask |= (1u << p.chip);

            if (got_mask == 0x3) {
                record.micro_timestamp = p.micro_timestamp;
                sd_signal_count++;

                // Detect a leads-off / saturated ECG: the AD8232 output pegs
                // at the ADC rail when an electrode loses contact.
                {
                    int16_t ecg = record.channels[ECG_SIGNAL_CHANNEL];
                    bool railed = (ecg >= (32767 - ECG_RAIL_MARGIN)) ||
                                  (ecg <= (-32768 + ECG_RAIL_MARGIN));
                    if (railed) {
                        if (++rail_count >= ECG_RAIL_SAMPLES && !leads_off) {
                            leads_off = true;
                            ESP_LOGW(TAG, "LEADS OFF? ECG ch%d railed at %d for >= %d samples (~%d ms)",
                                     ECG_SIGNAL_CHANNEL, ecg, ECG_RAIL_SAMPLES,
                                     ECG_RAIL_SAMPLES * 1000 / 357);
                        }
                    } else {
                        if (leads_off) {
                            ESP_LOGI(TAG, "ECG signal recovered (ch%d = %d)",
                                     ECG_SIGNAL_CHANNEL, ecg);
                            leads_off = false;
                        }
                        rail_count = 0;
                    }
                }

#ifdef  TEST_ECG_CSV
                if (csv != NULL) {
                    // Formats as: HH:MM:SS,Microseconds,Ch0,Ch1,Ch2,Ch3
                    // Retry a few times on a transient SD write glitch.
                    int nw = -1;
                    for (int attempt = 0; attempt < CSV_WRITE_RETRIES && nw < 0; attempt++) {
                        nw = fprintf(csv, "%02d:%02d:%02d,%llu,%d,%d,%d,%d\n",
                                global_wall_clock.tm_hour,
                                global_wall_clock.tm_min,
                                global_wall_clock.tm_sec,
                                record.micro_timestamp,
                                record.channels[0],
                                record.channels[1],
                                record.channels[2],
                                record.channels[3]);
                        if (nw < 0 && attempt + 1 < CSV_WRITE_RETRIES) {
                            clearerr(csv);
                            vTaskDelay(pdMS_TO_TICKS(CSV_RETRY_DELAY_MS));
                        }
                    }
                    if (nw < 0) {
                        if (csv_errors == 0) {
                            ESP_LOGE(TAG, "fprintf to CSV failed");
                        }
                        csv_errors++;
                    } else {
                        csv_records++;
                    }
                }
#endif
#ifdef  TEST_ECG_ISHNE
                if (ishne_active) {
                    int16_t ishne_buf[ISHNE_NLEADS];
                    for (int l = 0; l < ISHNE_NLEADS; l++) {
                        ishne_buf[l] = record.channels[ishne_lead_channel[l]];
                    }
                    if (ishne_write(&ishne, ishne_buf, 1) != 0) {
                        ESP_LOGE(TAG, "ISHNE write failed");
                    }
                }
#endif
#ifdef  PKG_SAMPLES_QRS
                // Skip QRS processing while motion is detected: RR intervals
                // computed from motion artifact would be meaningless. The raw
                // ECG is still recorded to CSV + ISHNE regardless.
                if (!motion_artifact_active) {
                    AppendToECGPacket(&record);
                }
#endif
                got_mask = 0;
            }

#ifdef  TEST_ECG_CSV
            // Flush periodically (not per record) so data reaches the card
            // without throttling the writer. fflush() alone only moves the
            // newlib buffer into the FATFS sector cache; fsync() forces FATFS
            // to commit the data and the directory entry (file size) to the
            // SD card so it survives a power-off/reset.
            if (csv != NULL && (xTaskGetTickCount() - last_flush) > pdMS_TO_TICKS(1000)) {
                fflush(csv);
                if (fsync(fileno(csv)) != 0) {
                    ESP_LOGE(TAG, "fsync(ecg_trace.csv) failed");
                }
                last_flush = xTaskGetTickCount();
            }
#endif

#ifdef  TEST_ECG_CSV
            // Fixed-duration recording: once the window elapses, flush and
            // close the file so it is complete on the card, then park the task.
            if (csv != NULL) {
                TickType_t elapsed = xTaskGetTickCount() - record_start;
                if (elapsed >= pdMS_TO_TICKS(TEST_RECORD_SECONDS * 1000)) {
                    fflush(csv);
                    fsync(fileno(csv));
                    fseek(csv, 0L, SEEK_END);
                    long csv_end_size = ftell(csv);
                    fclose(csv);
                    csv = NULL;
#ifdef  TEST_ECG_ISHNE
                    if (ishne_active) {
                        uint32_t ishne_samples = ishne.sample_count;
                        if (ishne_close(&ishne) == 0) {
                            ESP_LOGI(TAG, "ISHNE file closed (%lu samples/lead)",
                                     (unsigned long)ishne_samples);
                        } else {
                            ESP_LOGE(TAG, "ISHNE close failed");
                        }
                        ishne_active = false;
                    }
#endif
                    ESP_LOGI(TAG, "*** RECORDING COMPLETE: %ld records, %ld write errors, file %ld -> %ld bytes. You may move now. ***",
                             csv_records, csv_errors, csv_start_size, csv_end_size);
                    break;
                }
                if ((xTaskGetTickCount() - last_status) >= pdMS_TO_TICKS(10000)) {
                    int remaining = TEST_RECORD_SECONDS - (int)(elapsed / pdMS_TO_TICKS(1000));
                    ESP_LOGI(TAG, "Recording... %d s remaining", remaining);
                    last_status = xTaskGetTickCount();
                }
            }
#endif
        }
    }

    // Capture window finished: park the task so the closed file is left
    // intact while the rest of the system keeps running.
    ESP_LOGI(TAG, "%s: recording finished -- idle spin", __func__);
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/**
 * @brief Sample the MPU6050 and drive motion-artifact gating.
 *
 * Runs at ~100 Hz, writes a motion reference to /sdcard/motion.csv (microsecond
 * timestamp + raw ax/ay/az) for offline adaptive filtering, and toggles the
 * global motion_artifact_active flag used to gate on-device QRS processing.
 */
static void mpu6050_monitor_task(void *pv)
{
    (void)pv;
    mpu6050_raw_t r;
    int on_count = 0, off_count = 0;
    TickType_t last_flush = xTaskGetTickCount();

    FILE *motion_csv = fopen(MOUNT_POINT "/motion.csv", "a");
    if (motion_csv != NULL) {
        setvbuf(motion_csv, NULL, _IOFBF, 8192);
        ESP_LOGI(TAG, "Motion reference logging -> " MOUNT_POINT "/motion.csv");
    } else {
        ESP_LOGW(TAG, "Failed to open " MOUNT_POINT "/motion.csv");
    }

    while (1) {
        if (mpu6050_read_raw(mpu6050_handle, &r) == ESP_OK) {
            float ax = r.accel_x / MPU6050_ACCEL_LSB_PER_G;
            float ay = r.accel_y / MPU6050_ACCEL_LSB_PER_G;
            float az = r.accel_z / MPU6050_ACCEL_LSB_PER_G;
            float mag = sqrtf(ax * ax + ay * ay + az * az);
            float dyn = fabsf(mag - 1.0f);   /* "dynamic" acceleration, gravity removed */

            if (dyn > MOTION_THRESHOLD_G) {
                if (++on_count >= MOTION_ON_COUNT && !motion_artifact_active) {
                    motion_artifact_active = true;
                    ESP_LOGW(TAG, "MOTION ARTIFACT: dynamic accel %.3f g", (double)dyn);
                }
                off_count = 0;
            } else {
                if (++off_count >= MOTION_OFF_COUNT && motion_artifact_active) {
                    motion_artifact_active = false;
                    ESP_LOGI(TAG, "Motion cleared (%.3f g)", (double)dyn);
                }
                on_count = 0;
            }

            if (motion_csv != NULL) {
                fprintf(motion_csv, "%llu,%d,%d,%d\n",
                        (unsigned long long)esp_timer_get_time(),
                        r.accel_x, r.accel_y, r.accel_z);
            }
        } else {
            ESP_LOGW(TAG, "MPU6050 read failed");
        }

        if (motion_csv != NULL && (xTaskGetTickCount() - last_flush) > pdMS_TO_TICKS(1000)) {
            fflush(motion_csv);
            fsync(fileno(motion_csv));
            last_flush = xTaskGetTickCount();
        }
        vTaskDelay(pdMS_TO_TICKS(MOTION_SAMPLE_INTERVAL_MS));
    }
}

/**
 * @brief Setup_ADC_SD: kick off the ADC and SD tasks.
 * 
 */
void Setup_ADC_SD(void)
{
    esp_err_t   retv;
    static ads_chip_ctx_t chip_ctx[2];

    ESP_LOGI(TAG, "%s: called", __func__);
    if(bEnableADS1115 == false)
    {
        ESP_LOGI(TAG,"%s: ADS1115 is not enabled! -- exiting", __func__);    
    }

    // ALERT/RDY is open-drain active-low; pull up so each line idles high and
    // only pulses low when a conversion completes.
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_NEGEDGE,
        .pin_bit_mask = (1ULL << ADS_ALERT_GPIO_A) | (1ULL << ADS_ALERT_GPIO_B),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE
    };
    gpio_config(&io_conf);

    ads_rdy_semA = xSemaphoreCreateBinary();
    ads_rdy_semB = xSemaphoreCreateBinary();
    adc_queue = xQueueCreate(200, sizeof(ads_pair_t));

    retv = gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    if(retv != ESP_OK)
    {
        ESP_LOGD(TAG, "error from gpio_install_isr_service = %d(%s)", retv, esp_err_to_name(retv));
    }
    else
    {
        ESP_LOGI(TAG, "gpio_install_isr_service OK");
    }

    retv = gpio_isr_handler_add(ADS_ALERT_GPIO_A, ads_gpio_isr_handler, (void*)ads_rdy_semA);
    if(retv != ESP_OK)
    {
        ESP_LOGD(TAG, "error from gpio_isr_handler_add(GPIO %d) = %d(%s)", ADS_ALERT_GPIO_A, retv, esp_err_to_name(retv));
    } else {
        ESP_LOGI(TAG, "gpio_isr_handler_add OK on GPIO %d", ADS_ALERT_GPIO_A);
    }

    retv = gpio_isr_handler_add(ADS_ALERT_GPIO_B, ads_gpio_isr_handler, (void*)ads_rdy_semB);
    if(retv != ESP_OK)
    {
        ESP_LOGD(TAG, "error from gpio_isr_handler_add(GPIO %d) = %d(%s)", ADS_ALERT_GPIO_B, retv, esp_err_to_name(retv));
    } else {
        ESP_LOGI(TAG, "gpio_isr_handler_add OK on GPIO %d", ADS_ALERT_GPIO_B);
    }

    chip_ctx[0] = (ads_chip_ctx_t){ &ECGDev_ads1115,   ads_rdy_semA, 0, ADS_ALERT_GPIO_A };
    chip_ctx[1] = (ads_chip_ctx_t){ &ECGDev_ads1115_b, ads_rdy_semB, 1, ADS_ALERT_GPIO_B };

    // High-priority capture on cores 0/1; file management on core 1.
    xTaskCreatePinnedToCore(adc_acquisition_task, "ECG_AcqA", 8192, &chip_ctx[0], 10, NULL, 0);
    xTaskCreatePinnedToCore(adc_acquisition_task, "ECG_AcqB", 8192, &chip_ctx[1], 10, NULL, 1);
    xTaskCreatePinnedToCore(sd_writer_task, "SD_Write", 4096, NULL, 4, NULL, 1);

    // Motion reference + artifact gating (only if the MPU6050 initialized).
    if (bMPU6050) {
        xTaskCreatePinnedToCore(mpu6050_monitor_task, "MPU_Mon", 4096, NULL, 6, NULL, 1);
    }
}

void    post_sta_connected(void)
{
    fetch_sntp_time();              // ← ADD THIS - starts SNTP and blocks until synced
    // Now, we can sync up the RTC...
    sync_ntp_to_ds3231(&DS3231_dev);

    // RTC is synced; Wi-Fi is no longer needed. Turn it off to save power and
    // keep the device off the network for the rest of the session.
    wifi_shutdown();

    // WOW! This is wrong!!! I believe some of this did come 
    // from AI recs, will need to go back over sometime.
    //Init_sdmmc();
    //   InitSD_SPI();
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


    // change the order of initializations - ruling out problems regarding the uSD 
#if SD_DEBUG_INTERACTIVE
    InitSD_SPI_debug();
#else
    InitSD_SPI();
#endif

    // unscientific hunch, but putting in a slight delay to insure synchronization
    vTaskDelay(200/portTICK_PERIOD_MS);
   
    // Initialize WiFi - we need WiFi enabled in order to do NTP synchronization.
    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();

    /*
    fetch_sntp_time();              // ← ADD THIS - starts SNTP and blocks until synced
   
    // Now, we can sync up the RTC...
    sync_ntp_to_ds3231(&DS3231_dev);
     */
    ESP_LOGI(TAG," Done with wifi_init_sta ");

    while(bWifi_Connected == false)
    {
        ESP_LOGI(TAG,"waiting for STA_CONNECTED...");
        
        vTaskDelay(2000/portTICK_PERIOD_MS);
    }
    post_sta_connected();

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
    // The SD writer task handles the fixed-duration recording and then parks
    // itself; app_main just prints a heartbeat so the operator can see that
    // acquisition is still alive.
    while(1)
    {
        // This is a visual way of confirming that the ADC capture is functioning... or not.
        ESP_LOGI(TAG, "Running... SD count = %d, ADC count = %d  ISR count = %lu",sd_signal_count,adc_signal_count,isr_count);
        vTaskDelay(2000/portTICK_PERIOD_MS);
    }
}

