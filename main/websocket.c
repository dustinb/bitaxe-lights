#include "esp_log.h"
#include "lights.h"
#include "wifi_creds.h"
#include <cJSON.h>
#include "esp_event_base.h"
#include "esp_http_client.h"
#include "freertos/task.h"
#include "freertos/FreeRTOS.h"
#include "esp_websocket_client.h"

static const char *TAG = "WEBSOCKET";

static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    switch (event_id)
    {
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_CONNECTED");
        break;
    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_DISCONNECTED");
        break;
    case WEBSOCKET_EVENT_DATA:
        ESP_LOGD(TAG, "Received opcode: %d", data->op_code);

        if (data->op_code == 0x01 || data->op_code == 0x02)
        { // Text or Binary frame
            queue_led_event(data->data_ptr);
        }
        break;
    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_ERROR");
        break;
    }
}

void websocket_init(void *pvParameters)
{
    esp_websocket_client_config_t config = {
        .uri = GO_SERVER_WS,
        .reconnect_timeout_ms = 5000,
        .network_timeout_ms = 3000,
        .buffer_size = 1024, // Set receive buffer size (in bytes)
    };

    esp_websocket_client_handle_t client = esp_websocket_client_init(&config);
    esp_websocket_register_events(client, WEBSOCKET_EVENT_ANY, websocket_event_handler, (void *)client);
    esp_websocket_client_start(client);
    vTaskDelay(4000 / portTICK_PERIOD_MS);

    while (1)
    {
        vTaskDelay(30000 / portTICK_PERIOD_MS);
    }

    // Note: This code will never be reached unless the while loop is broken
    esp_websocket_client_close(client, 1000);
    esp_websocket_client_destroy(client);
}