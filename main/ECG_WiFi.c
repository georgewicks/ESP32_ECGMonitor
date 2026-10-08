/**
 * @file ECG_WiFi.c
 * @author george.r.wicks@gmail.com
 * @brief   In order to use NTP to sync the DS3231, we do need to enable WiFI. We will be using code 
 * from the ESP-IDF examples folder, specifically examples/wifi/getting_started/station/main/station_example_main.c
 * @version 0.1
 * @date 2026-09-01
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif_sntp.h"
#include "esp_sntp.h"

#define WIFI_SSID      "GandInet"
#define WIFI_PASS      "G051112i"

// FreeRTOS event group to signal when we are connected to Wi-Fi
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

bool bWifi_Connected = false; // Global flag to indicate Wi-Fi connection status

static bool s_wifi_stopping = false; // set before a deliberate esp_wifi_stop()

static const char *TAG = "ECGMon-WIFI";

// Optional callback triggered when SNTP actually syncs the system time
void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Notification: System time synchronized successfully!");
}

// 2. The SNTP Fetch Sequence
void fetch_sntp_time(void)
{
    ESP_LOGI(TAG, "Initializing SNTP Service...");

     // Corrected type name for ESP-IDF v6.0
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    config.sync_cb = time_sync_notification_cb;

    // Initialize and start SNTP
    ESP_ERROR_CHECK(esp_netif_sntp_init(&config));

    // Set Timezone (Example: US Central Time)
    // Replace with your region's TZ string
    setenv("TZ", "CST6CDT,M3.2.0,M11.1.0", 1);
    tzset();

    // Wait for the time to synchronize over the network
    int retry = 0;
    const int retry_count = 10;
    while (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(2000)) == ESP_ERR_TIMEOUT && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for network time sync... (%d/%d)", retry, retry_count);
    }

    // Print out the newly synchronized local time
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    char strftime_buf[64];
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "Synchronized Date/Time: %s", strftime_buf);
}

/**
 * @brief  Turn Wi-Fi off once NTP has synced the RTC.
 *
 * The device only needs Wi-Fi for the one-time SNTP sync at boot. Leaving the
 * radio on wastes battery and keeps the device present on the network, which
 * is undesirable for a medical device. esp_wifi_stop() also fires
 * WIFI_EVENT_STA_DISCONNECTED, so s_wifi_stopping suppresses the auto-reconnect.
 */
void wifi_shutdown(void)
{
    if (s_wifi_stopping) {
        return;
    }
    s_wifi_stopping = true;
    esp_wifi_stop();
    ESP_LOGI(TAG, "Wi-Fi stopped after NTP sync (power saving)");
}

// 1. Wi-Fi and IP Event Handler
static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *disconn = (wifi_event_sta_disconnected_t *)event_data;
        ESP_LOGI(TAG, "Wi-Fi disconnected, reason=%d", disconn->reason);

        bWifi_Connected = false;
        if (s_wifi_stopping) {
            // Deliberate shutdown after NTP sync; don't reconnect.
            return;
        }

        ESP_LOGI(TAG, "Disconnected from AP. Retrying...");
        esp_wifi_connect();
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) { // Fixed here
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Successfully acquired IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        bWifi_Connected = true;
    }
}
#define DEFAULT_SCAN_LIST_SIZE 10

static void wifi_scan(void)
{
    uint16_t number = DEFAULT_SCAN_LIST_SIZE;
    wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
    uint16_t ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));

    //esp_err_t scan_ret = esp_wifi_scan_start(NULL, true);
    //ESP_LOGI(TAG, "scan_start returned: %s", esp_err_to_name(scan_ret));
    // esp_wifi_scan_start(NULL, true);  // blocking scan, all channels
    wifi_scan_config_t scan_config = {
        .show_hidden = true,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active.min = 100,
        .scan_time.active.max = 300,
    };
    esp_wifi_scan_start(&scan_config, true);

    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));

    ESP_LOGI(TAG, "Total APs scanned = %u", ap_count);
    for (int i = 0; i < number; i++) {
        ESP_LOGI(TAG, "SSID \t\t%s", ap_info[i].ssid);
        ESP_LOGI(TAG, "RSSI \t\t%d", ap_info[i].rssi);
        ESP_LOGI(TAG, "Channel \t\t%d", ap_info[i].primary);
    }
}

void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    // Initialize the network interface layer
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta(); // Explicitly create the Wi-Fi Station instance

    // Setup Wi-Fi driver allocation
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event listeners
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    // Register for all Wi-Fi events
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID, // Correct global wildcard
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));

    // Register explicitly for the "Got IP" station event
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP, // Explicit event ID
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    // Configure credentials
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    ESP_LOGI(TAG, " **** *** SSID = %s", wifi_config.sta.ssid);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    //ESP_ERROR_CHECK(esp_wifi_set_inactive_time(WIFI_IF_STA,20)); // Set inactive time to 20 seconds
 
    
    // --- Run scan here, before waiting for connection ---
    wifi_scan();

    ESP_LOGI(TAG, "Waiting for connection");

    // Block here until the event loop confirms we have an IP
    xEventGroupWaitBits(s_wifi_event_group,
                        WIFI_CONNECTED_BIT,
                        pdFALSE,
                        pdTRUE,
                        portMAX_DELAY);

    ESP_LOGI(TAG, "Connection completed (supposedly)");
    

}

void do_sntp_app_main(void)
{
    // NVS flash must be initialized for Wi-Fi storage data
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 1. Block until Wi-Fi is fully operational and has an IP Address
    wifi_init_sta();

    // 2. Network is alive, now execute SNTP
    fetch_sntp_time();
}
