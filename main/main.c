#include "esp_log.h"
#include "lights.h"
#include "nvs_flash.h"
#include "wifi_manager.h"

static const char *TAG = "BITAXE_LED";

void app_main(void)
{
    ESP_LOGI(TAG, "Starting BITAXE_LED");
    ESP_ERROR_CHECK(nvs_flash_init());

    ESP_LOGI(TAG, "Initializing lights");
    lights_init();

    ESP_LOGI(TAG, "Initializing WiFi");
    wifi_init_sta();
}
