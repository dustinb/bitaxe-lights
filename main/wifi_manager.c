#include "wifi_manager.h"
#include "websocket.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_event_base.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "config_manager.h"
#include <string.h>

static const char *TAG = "WIFI_MANAGER";

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        esp_wifi_connect();
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP:" IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "Initializing Go websocket");
        xTaskCreate(&websocket_init, "websocket_init", 8192, NULL, 5, NULL);
    }
}

void wifi_init_sta(void)
{
    device_config_t config;
    if (!config_manager_load(&config)) {
        ESP_LOGE(TAG, "Failed to load WiFi configuration");
        return;
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .threshold.rssi = -127,
            .pmf_cfg = {
                .capable = true,
                .required = false},
        },
    };

    // Copy the SSID and password from the stored configuration
    strncpy((char *)wifi_config.sta.ssid, config.wifi_ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, config.wifi_password, sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}