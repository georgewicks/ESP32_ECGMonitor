/**
 * @file I2C_Handling.c
 * @author george.r.wicks@gmail.com
 * @brief  I2C Functions 
 * @version 0.1
 * @date 2026-08-25
 * 
 * @copyright
 * 
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

#include <esp_log.h>

#include "common.h"


#include "spoof_i2cdev.h"

// device addresses we need to check for.
#include "ads111x.h"
#include "ds3231.h"

static const char *TAG = "I2C_Handling";

extern SemaphoreHandle_t	xMutex;
extern i2c_master_bus_handle_t bus_handle;

#ifdef	OLDER
extern i2c_master_dev_handle_t dev_handle;
#endif

#define SDA_IO          (GPIO_I2C_SDA)                      /*!< gpio number for I2C master data  */
#define SCL_IO          (GPIO_I2C_SCL)                      /*!< gpio number for I2C master clock */

#define FREQ_HZ         (100000)                 /*!< I2C master clock frequency */
#define TX_BUF_DISABLE  (0)               /*!< I2C master doesn't need buffer */
#define RX_BUF_DISABLE  (0)               /*!< I2C master doesn't need buffer */

#define I2C_NUM             I2C_NUM_0                /*!< I2C number */
#define I2C_MODE            I2C_MODE_MASTER         /*!< I2C mode to act as */
#define I2C_RX_BUF_STATE    RX_BUF_DISABLE  /*!< I2C set rx buffer status */
#define I2C_TX_BUF_STATE    TX_BUF_DISABLE  /*!< I2C set tx buffer status */
#define I2C_INTR_ALOC_FLAG  (0)           /*!< I2C set interrupt allocation flag */


#define OUTPUT_SCAN             true
#define OUTPUT_SCAN_SILENT      false
#define MAX_I2C_DEV             (128)

#define I2C_MASTER_NUM I2C_NUM_0  // Uses I2C port 0

bool    i2c_device_list[MAX_I2C_DEV];

bool    IsADS1115Present(void)
{
    // Currently we will only look for one specific address
    return i2c_device_list[ADS111X_ADDR_GND];
}

bool    IsADS1115BPresent(void)
{
    return i2c_device_list[ADS111X_ADDR_B];
}

bool    IsDS3231Present(void)
{
    return i2c_device_list[DS3231_ADDR];
}

bool    IsMPU6050Present(void)
{
    return i2c_device_list[MPU6050_ADDR];
}

bool    IsScanOutput = OUTPUT_SCAN;

bool    bDS3231 = false;        // Is there a DS3231 RTC?
bool    bADS1115 = false;       // Is there (one or more) ADS1115 ADC chip?

const	TickType_t	ScanPeriod = (((5000 / portTICK_PERIOD_MS) * 12) * 50);

// ESP Docs AI Assistant suggestions

// Device handles - one per device
i2c_master_dev_handle_t ads1115_handle;  // 0x48
i2c_master_dev_handle_t gpio_exp_handle; // 0x27  PCF8574
i2c_master_dev_handle_t eeprom_handle;   // 0x57
i2c_master_dev_handle_t rtc_handle;      // 0x68
i2c_master_dev_handle_t ads1115_b_handle; // 0x49  second ADS1115
i2c_master_dev_handle_t mpu6050_handle;  // 0x69  MPU6050 (AD0 -> VDD)

/**
 * @brief i2c master initialization
 */
void i2c_master_init(void)
{
	esp_err_t	errval;

	ESP_LOGI(TAG, "%s: called", __FILE__);

	i2c_master_bus_config_t i2c_bus_config = {
		.clk_source = I2C_CLK_SRC_DEFAULT,
		.i2c_port = I2C_NUM_0,
		.sda_io_num = GPIO_I2C_SDA,
		.scl_io_num = GPIO_I2C_SCL,
		.glitch_ignore_cnt = 7,
	};
	errval = i2c_new_master_bus(&i2c_bus_config, &bus_handle);
	if(errval != ESP_OK)
	{
		ESP_LOGE(TAG,"error from i2c_new_master_bus = %d (%s)",errval, esp_err_to_name(errval));
	}

	ESP_LOGI(TAG, "&bus_handle = %p", (void*)&bus_handle);

	// TODO: MUST do next
	// create multiple i2c_device_config_t config structs, each device must have a unique device
	// structure. 
	//	ADS1115			0x48
	//	PCF8574			0x27
	//  DS3231			0x57 for the EEprom, 0x68 for the RTC part.
	//


	// ESP Docs AI Assistant suggestions
	// ADS1115 - no clock stretching needed; 1 MHz fast-mode-plus is supported.
	i2c_device_config_t ads_cfg = {
		.dev_addr_length = I2C_ADDR_BIT_LEN_7,
		.device_address = 0x48,
		.scl_speed_hz = 1000000,
		.scl_wait_us = 5000,					// as per Google AI  2026-10-01
	};
	i2c_master_bus_add_device(bus_handle, &ads_cfg, &ads1115_handle);

	ESP_LOGI(TAG, "ads1115_handle = %p", (void*)ads1115_handle);

	// Second ADS1115 at 0x49 (ADDR -> VDD), same 1 MHz fast-mode-plus.
	i2c_device_config_t ads_b_cfg = {
		.dev_addr_length = I2C_ADDR_BIT_LEN_7,
		.device_address = ADS111X_ADDR_B,
		.scl_speed_hz = 1000000,
		.scl_wait_us = 5000,
	};
	i2c_master_bus_add_device(bus_handle, &ads_b_cfg, &ads1115_b_handle);
	ESP_LOGI(TAG, "ads1115_b_handle = %p", (void*)ads1115_b_handle);

	// GPIO Expander
	i2c_device_config_t gpio_exp_cfg = {
		.dev_addr_length = I2C_ADDR_BIT_LEN_7,
		.device_address = 0x27,
		.scl_speed_hz = 100000,
		.scl_wait_us = 0,
	};
	i2c_master_bus_add_device(bus_handle, &gpio_exp_cfg, &gpio_exp_handle);

	ESP_LOGI(TAG, "gpio_exp_handle = %p", (void*)gpio_exp_handle);

	// Device at 0x57
	i2c_device_config_t dev57_cfg = {
		.dev_addr_length = I2C_ADDR_BIT_LEN_7,
		.device_address = 0x57,
		.scl_speed_hz = 100000,
		.scl_wait_us = 0,
	};
	i2c_master_bus_add_device(bus_handle, &dev57_cfg, &eeprom_handle);

	// MCP9808 or RTC at 0x68 - with clock stretching
	i2c_device_config_t rtc_cfg = {
		.dev_addr_length = I2C_ADDR_BIT_LEN_7,
		.device_address = 0x68,
		.scl_speed_hz = 100000,
		.scl_wait_us = 20000, // keep clock stretching only where needed
	};
	i2c_master_bus_add_device(bus_handle, &rtc_cfg, &rtc_handle);

	// MPU6050 at 0x69 (AD0 -> VDD because the DS3231 owns 0x68). It is also a
	// clock-stretcher, so allow a generous scl_wait_us.
	i2c_device_config_t mpu6050_cfg = {
		.dev_addr_length = I2C_ADDR_BIT_LEN_7,
		.device_address = MPU6050_ADDR,
		.scl_speed_hz = 400000,
		.scl_wait_us = 20000,
	};
	i2c_master_bus_add_device(bus_handle, &mpu6050_cfg, &mpu6050_handle);


    vTaskDelay(1000/portTICK_PERIOD_MS);

}

/**
 * @brief I2C Scanner Task. It will run every ScanPeriod. It has two modes: print the output, or silent, which just marks the
 * occurence in the i2c_device_list[]
 * 
 * Having this as a Task can cause problems with synchronization: This can interrupt another process & I need to rule that out 
 * I will make a single function call that is called once
 * 
 */

//static void i2c_scanner_task(void *pvParameters) {
static void i2c_scanner_task(void) {

    ESP_LOGI(TAG, "Starting I2C scanner on bus %d...", I2C_MASTER_NUM);
    
    //while (1) {
    	vTaskDelay(1000/portTICK_PERIOD_MS);

        if( IsScanOutput )
        {
            ESP_LOGI(TAG, "\n     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f");
        }
        int device_count = 0;

		if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE)
		{

			for (int i = 0; i < 128; i += 16)
			{
				if(IsScanOutput) printf("%02x: ", i);
				for (int j = 0; j < 16; j++)
				{
					uint8_t address = i + j;
					if (address == 0)
					{
						if(IsScanOutput) printf("-- ");
						continue;
					}

					// Probe the address with a 50ms timeout
					esp_err_t ret = i2c_master_probe(bus_handle, address, (1000 / portTICK_PERIOD_MS));

					if (ret == ESP_OK)
					{
						if(IsScanOutput) printf("%02x ", address);
                        i2c_device_list[address] = true;
						device_count++;
					}
					else
					{
                        if(ret == ESP_ERR_TIMEOUT)
                            printf("HW Err ");
						if(IsScanOutput) printf("-- ");
                        i2c_device_list[address] = false;
					}


					fflush(stdout);
				}
				if(IsScanOutput) printf("\n");
			}
		}
        xSemaphoreGive(xMutex);

		//if(IsScanOutput) ESP_LOGI(TAG, "Scan complete. Found %d device(s). Waiting %d seconds...\n", device_count, (int)ScanPeriod);


        //vTaskDelay(ScanPeriod);
	//}
	vTaskDelay(1000 / portTICK_PERIOD_MS);

}


void    InitI2C(void)
{
    ESP_LOGI(TAG, "%s: starting", __func__);
    vTaskDelay(500 / portTICK_PERIOD_MS);

    // Setup I2C
    i2c_master_init();

    // run I2C scanner
    //xTaskCreate(i2c_scanner_task, "i2c_scanner", 4096, NULL, 5, NULL);
	i2c_scanner_task();		// No longer a task - run once

    vTaskDelay(1000 / portTICK_PERIOD_MS);
}

/**
 * @brief Basic API for device initiation.
 * 
 */

i2c_dev_t	ECGDev_ads1115;
i2c_dev_t	ECGDev_ads1115_b;

#if 0
esp_err_t	InitADS1115(void)
{
	esp_err_t retv = ESP_OK;
	if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE)
	{


		ESP_LOGI(TAG, "ads1115_handle passed to init_desc = %p", (void*)ads1115_handle);
		retv = ads111x_init_desc(&ECGDev_ads1115, ads1115_handle, ADS111X_ADDR_GND, I2C_NUM, 0, 0);
		ESP_LOGI(TAG, "ads111x_init_desc = %d(%s)", retv, esp_err_to_name(retv));

		// ESP Docs AI Assistant
		// Add these after your existing ADS1115 init:
		// Set Lo_thresh = 0x0000 and Hi_thresh = 0x8000
		// This enables ALERT/RDY as conversion-ready output

		uint16_t config_val = 0;
		esp_err_t ret = ads111x_read_reg(&ECGDev_ads1115, 0x01, &config_val);
		ESP_LOGI(TAG, "Config register = 0x%04x (expected 0x8583)", config_val);

		// Further ESP Docs AI Assistant suggestions based on debugging:
		ESP_LOGI(TAG, "Setting comp queue...");
		ads111x_set_comp_queue(&ECGDev_ads1115, ADS111X_COMP_QUEUE_1);

		ESP_LOGI(TAG, "Setting comp mode...");
		ads111x_set_comp_mode(&ECGDev_ads1115, ADS111X_COMP_MODE_WINDOW);

		ESP_LOGI(TAG, "Setting low threshold...");
		ads111x_set_comp_low_thresh(&ECGDev_ads1115, 0x0000);

		ESP_LOGI(TAG, "Setting high threshold...");
		ads111x_set_comp_high_thresh(&ECGDev_ads1115, 0x7FFF);
		ESP_LOGI(TAG, " set threshhold limits Done ");

		uint16_t lo_thresh = 0, hi_thresh = 0;
		ads111x_read_reg(&ECGDev_ads1115, 0x02, &lo_thresh);
		ads111x_read_reg(&ECGDev_ads1115, 0x03, &hi_thresh);
		ESP_LOGI(TAG, "Lo thresh readback = 0x%04X (expected 0x0000)", lo_thresh);
		ESP_LOGI(TAG, "Hi thresh readback = 0x%04X (expected 0x7FFF)", hi_thresh);

		xSemaphoreGive(xMutex);
	}


	return retv;
}
#endif

esp_err_t InitADS1115(void)
{
    esp_err_t retv = ESP_OK;
    if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE)
    {
        retv = ads111x_init_desc(&ECGDev_ads1115, ads1115_handle, 
                                  ADS111X_ADDR_GND, I2C_NUM, 0, 0);
        ESP_LOGI(TAG, "ads111x_init_desc = %d(%s)", retv, esp_err_to_name(retv));

        // Enable ALERT/RDY conversion-ready (RDY) mode:
        //   Lo_thresh MSB = 0 (0x0000), Hi_thresh MSB = 1 (0x8000).
        ESP_LOGI(TAG, "Setting low threshold...");
        ads111x_set_comp_low_thresh(&ECGDev_ads1115, 0x0000);

        ESP_LOGI(TAG, "Setting high threshold...");
        ads111x_set_comp_high_thresh(&ECGDev_ads1115, 0x8000);

        // Readback verification
        uint16_t lo = 0, hi = 0;
        ads111x_read_reg(&ECGDev_ads1115, 0x02, &lo);
        ads111x_read_reg(&ECGDev_ads1115, 0x03, &hi);
        ESP_LOGI(TAG, "Lo thresh readback = 0x%04X", lo);
        ESP_LOGI(TAG, "Hi thresh readback = 0x%04X", hi);

        xSemaphoreGive(xMutex);  // Inside the if block!
    }
    return retv;
}

esp_err_t InitADS1115B(void)
{
    esp_err_t retv = ESP_OK;
    if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE)
    {
        retv = ads111x_init_desc(&ECGDev_ads1115_b, ads1115_b_handle,
                                  ADS111X_ADDR_VCC, I2C_NUM, 0, 0);
        ESP_LOGI(TAG, "ads111x_init_desc(B) = %d(%s)", retv, esp_err_to_name(retv));

        // Hi_thresh MSB = 1 / Lo_thresh MSB = 0 enables ALERT/RDY RDY mode.
        ads111x_set_comp_low_thresh(&ECGDev_ads1115_b, 0x0000);
        ads111x_set_comp_high_thresh(&ECGDev_ads1115_b, 0x8000);

        xSemaphoreGive(xMutex);
    }
    return retv;
}

i2c_dev_t   DS3231_dev;

/**
 * @brief Initialize the DS3231
 * 
 */
esp_err_t   InitDS3231(void)
{
    esp_err_t  retv = ESP_OK;
    ESP_LOGI(TAG, "%s called", __func__);
	if (xSemaphoreTake(xMutex, portMAX_DELAY) == pdTRUE)
	{
		retv = ds3231_init_desc(&DS3231_dev, rtc_handle, I2C_NUM, 0, 0);
		if (retv != ESP_OK)
		{
			ESP_LOGE(TAG, "Error from ds3231_init_desc = %d(%s)", retv, esp_err_to_name(retv));
		}
	}
	xSemaphoreGive(xMutex);
	return retv;
}
