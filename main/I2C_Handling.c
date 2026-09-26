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
extern i2c_master_dev_handle_t dev_handle;

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

bool    IsDS3231Present(void)
{
    return i2c_device_list[DS3231_ADDR];
}

bool    IsScanOutput = OUTPUT_SCAN;

bool    bDS3231 = false;        // Is there a DS3231 RTC?
bool    bADS1115 = false;       // Is there (one or more) ADS1115 ADC chip?

const	TickType_t	ScanPeriod = (((5000 / portTICK_PERIOD_MS) * 12) * 10);



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

	i2c_device_config_t dev_cfg = {
		.dev_addr_length = I2C_ADDR_BIT_LEN_7,
		.device_address = ADS111X_ADDR_GND, 
		.scl_speed_hz = 100000,
		.scl_wait_us = 20000,							// the MCP9808 might need clock stretching.
	};
	errval = i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle);
	if(errval != ESP_OK)
	{
		ESP_LOGE(TAG,"error from i2c_master_bus_add_device = %d (%s)",errval, esp_err_to_name(errval));
	}
    vTaskDelay(200/portTICK_PERIOD_MS);

}

/**
 * @brief I2C Scanner Task. It will run every ScanPeriod. It has two modes: print the output, or silent, which just marks the
 * occurence in the i2c_device_list[]
 * 
 */

static void i2c_scanner_task(void *pvParameters) {

    ESP_LOGI(TAG, "Starting I2C scanner on bus %d...", I2C_MASTER_NUM);
    
    while (1) {

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

		if(IsScanOutput) ESP_LOGI(TAG, "Scan complete. Found %d device(s). Waiting %d seconds...\n", device_count, (int)ScanPeriod);


        vTaskDelay(ScanPeriod);
	}
}


void    InitI2C(void)
{
    ESP_LOGI(TAG, "%s: starting", __func__);

    // Setup I2C
    i2c_master_init();

    // run I2C scanner
    xTaskCreate(i2c_scanner_task, "i2c_scanner", 4096, NULL, 5, NULL);

    vTaskDelay(1000 / portTICK_PERIOD_MS);
}

/**
 * @brief Basic API for device initiation.
 * 
 */

i2c_dev_t	ECGDev_ads1115;

esp_err_t	InitADS1115(void)
{
	esp_err_t   retv = ads111x_init_desc( &ECGDev_ads1115, dev_handle, ADS111X_ADDR_GND, I2C_NUM, 0, 0);
	ESP_LOGI(TAG, "ads111x_init_desc = %d(%s)", retv, esp_err_to_name(retv));

	// ESP Docs AI Assistant
	// Add these after your existing ADS1115 init:
	// Set Lo_thresh = 0x0000 and Hi_thresh = 0x8000
	// This enables ALERT/RDY as conversion-ready output
	ESP_LOGI(TAG, " about to set threshhold limits");
	ads111x_write_reg(&ECGDev_ads1115, REG_THRESH_L, 0x0000);
	ads111x_write_reg(&ECGDev_ads1115, REG_THRESH_H, 0x8000);
	ESP_LOGI(TAG, " set threshhold limits Done ");

	return retv;
}

i2c_dev_t   DS3231_dev;

/**
 * @brief Initialize the DS3231
 * 
 */
esp_err_t   InitDS3231(void)
{
    esp_err_t  retv;
    ESP_LOGI(TAG, "%s called", __func__);
    retv = ds3231_init_desc( &DS3231_dev, dev_handle, I2C_NUM, 0, 0);
    if(retv != ESP_OK)
    {
        ESP_LOGE(TAG, "Error from ds3231_init_desc = %d(%s)", retv, esp_err_to_name(retv));
    }
	return retv;
}
