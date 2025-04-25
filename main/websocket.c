#include "esp_log.h"
#include "lights.h"
#include "config_manager.h"
#include <cJSON.h>
#include "esp_event_base.h"
#include "esp_http_client.h"
#include "freertos/task.h"
#include "freertos/FreeRTOS.h"
#include "esp_websocket_client.h"
#include <string.h>

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
        if (data->op_code == 0x01 || data->op_code == 0x02) // Text or Binary frame
        {
            cJSON *root = cJSON_Parse(data->data_ptr);
            if (root)
            {
                blink_event_t event = {0};
                cJSON *type = cJSON_GetObjectItem(root, "type");
                cJSON *segment = cJSON_GetObjectItem(root, "segment");
                cJSON *value = cJSON_GetObjectItem(root, "value");
                if (type && segment)
                {
                    ESP_LOGW(TAG, "Type: %s, Segment: %d, Value: %lld",
                             type->valuestring, segment->valueint,
                             value ? (int64_t)value->valuedouble : 0);

                    strncpy(event.type, type->valuestring, sizeof(event.type) - 1);
                    event.segment = segment->valueint;
                    event.value = value ? (int64_t)value->valuedouble : 0;
                    queue_lights_event(event);
                }
                cJSON_Delete(root);
            }
            else
            {
                ESP_LOGE(TAG, "Failed to parse JSON");
            }
        }
        break;
    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_ERROR");
        break;
    }
}

// {Segment:4 Type:mining.notify Value:}
void websocket_init(void *pvParameters)
{
    device_config_t nvsConfig;
    if (!config_manager_load(&nvsConfig)) {
        ESP_LOGE(TAG, "Failed to load websocket configuration");
        vTaskDelete(NULL);
        return;
    }

    // Format the websocket URI
    char uri[512];  // Adjust size if needed
    snprintf(uri, sizeof(uri), "ws://%s:8080/ws", nvsConfig.websocket_server);

    esp_websocket_client_config_t ws_config = {
        .uri = uri,
        .reconnect_timeout_ms = 300,
        .network_timeout_ms = 400,
        .buffer_size = 256,
        .disable_auto_reconnect = false,
        .transport = WEBSOCKET_TRANSPORT_OVER_TCP,
        .skip_cert_common_name_check = true,
        .ping_interval_sec = 1,
        .disable_pingpong_discon = true,
        .use_global_ca_store = true
    };

    esp_websocket_client_handle_t client = esp_websocket_client_init(&ws_config);
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