#include "esp_log.h"
#include "lights.h"
#include "nvs_flash.h"
#include "wifi_manager.h"
#include "config_manager.h"

static const char *TAG = "BITAXE_LED";

void app_main(void)
{
    ESP_LOGI(TAG, "Starting BITAXE_LED");
    
    // Initialize configuration system
    config_manager_init();
    
    // If we're not in AP mode, initialize the lights and WiFi
    if (config_manager_is_configured()) {
        ESP_LOGI(TAG, "Initializing lights");
        lights_init();

        ESP_LOGI(TAG, "Initializing WiFi");
        wifi_init_sta();
    }
}
