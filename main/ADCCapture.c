/**
 * @file ADCCapture.c
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2026-08-27
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include "ADCCapture.h"

extern esp_err_t ads111x_read_reg( i2c_dev_t *dev, uint8_t reg, uint16_t *val);
extern esp_err_t ads111x_write_reg(i2c_dev_t *dev, uint8_t reg, uint16_t val);

static QueueHandle_t adc_queue = NULL;
static SemaphoreHandle_t ads_rdy_semaphore = NULL;

// Global wall clock shared safely via thread updates
static volatile rtc_time_t global_wall_clock = {0};

static const uint16_t ads_mux_config[4] = {
    0xC4E8, // Ch 0, Single-Shot, 860 SPS, Alert Active Low
    0xD4E8, // Ch 1, Single-Shot, 860 SPS, Alert Active Low
    0xE4E8, // Ch 2, Single-Shot, 860 SPS, Alert Active Low
    0xF4E8  // Ch 3, Single-Shot, 860 SPS, Alert Active Low
};

// Helper: Convert BCD format from DS3231 to regular integers
static uint8_t bcd_to_dec(uint8_t val) {
    return ((val / 16) * 10) + (val % 16);
}

#if 0

static esp_err_t ds3231_read_time(rtc_time_t *time) {
    uint8_t reg = DS3231_TIME_REG;
    uint8_t data[3];
    esp_err_t err = i2c_master_write_read_device(I2C_MASTER_NUM, DS3231_I2C_ADDR, &reg, 1, data, 3, pdMS_TO_TICKS(10));
    if (err == ESP_OK) {
        time->second = bcd_to_dec(data[0] & 0x7F);
        time->minute = bcd_to_dec(data[1] & 0x7F);
        time->hour   = bcd_to_dec(data[2] & 0x3F); // 24-hour mode assumption
    }
    return err;
}

// TODO: Check to see: Ruslan might have produced 16-bit read and write generic functions to the ADS1115
// they might be static to the ads111x module, BUT we can simply make externally available
// doing direct 16-bit calls would make the ADC handling faster than reading a 16-bit register, and masking and OR'ing bits,
// then writing it back out

static esp_err_t ads1115_write_reg(uint8_t reg, uint16_t val) {
    uint8_t out_buf[3] = { reg, (uint8_t)(val >> 8), (uint8_t)(val & 0xFF) };
    return i2c_master_write_to_device(I2C_MASTER_NUM, ADS1115_I2C_ADDR, out_buf, 3, pdMS_TO_TICKS(10));
}

static esp_err_t ads1115_read_reg(uint8_t reg, uint16_t *val) {
    uint8_t in_buf[2];
    esp_err_t err = i2c_master_write_read_device(I2C_MASTER_NUM, ADS1115_I2C_ADDR, &reg, 1, in_buf, 2, pdMS_TO_TICKS(10));
    if (err == ESP_OK) {
        *val = (in_buf[0] << 8) | in_buf[1];
    }
    return err;
}
#endif

static void IRAM_ATTR ads_gpio_isr_handler(void* arg) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(ads_rdy_semaphore, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

// CORE 0: High-Priority ECG Acquisition
void adc_acquisition_task(void *pvParameters) {
    uint8_t tracking_channel = 0;
    adc_sample_t current_batch = {0};
    uint16_t raw_reading = 0;

    i2c_dev_t   ADS1115;

    //ads1115_write_reg(ADS1115_CFG_REG, ads_mux_config[tracking_channel]);


    while (1) {
        if (xSemaphoreTake(ads_rdy_semaphore, portMAX_DELAY) == pdTRUE) {
            
            if (ads1115_read_reg(ADS1115_CONV_REG, &raw_reading) == ESP_OK) {
                current_batch.channels[tracking_channel] = (int16_t)raw_reading;
            }

            uint8_t next_channel = (tracking_channel + 1) % 4;

            if (tracking_channel == 3) {
                // High-precision microsecond hardware timer stamp for ECG baseline alignment
                current_batch.micro_timestamp = esp_timer_get_time();
                xQueueSend(adc_queue, &current_batch, 0);
            }

            tracking_channel = next_channel;
            ads1115_write_reg(ADS1115_CFG_REG, ads_mux_config[tracking_channel]);
        }
    }
}

struct tm   global_wall_clock;

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

void adc_app_main(void) {

    // Install I2C will be done before this...

    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_NEGEDGE,
        .pin_bit_mask = (1ULL << ADS_ALERT_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE
    };
    gpio_config(&io_conf);
    
    ads_rdy_semaphore = xSemaphoreCreateBinary();
    adc_queue = xQueueCreate(100, sizeof(adc_sample_t)); // Expanded queue size for dense trace data

    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_isr_handler_add(ADS_ALERT_GPIO, ads_gpio_isr_handler, NULL);

    // Pin high priority capture to Core 0, file management to Core 1
    xTaskCreatePinnedToCore(adc_acquisition_task, "ECG_Acq", 4096, NULL, 10, NULL, 0);
    xTaskCreatePinnedToCore(sd_writer_task, "SD_Write", 4096, NULL, 4, NULL, 1);
}
