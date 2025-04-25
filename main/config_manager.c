#include "config_manager.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include <string.h>
#include <sys/param.h>

static inline int min(int a, int b) { return a < b ? a : b; }

static const char *TAG = "CONFIG_MANAGER";
static const char *CONFIG_NAMESPACE = "device_config";
static const char *CONFIG_KEY = "config_data";

static httpd_handle_t server = NULL;
static device_config_t current_config;

static const char *HTML_FORM = 
    "<!DOCTYPE html><html><head><title>BITAXE LED Configuration</title>"
    "<style>body{font-family:Arial,sans-serif;max-width:600px;margin:0 auto;padding:20px}"
    "input{width:100%;padding:8px;margin:5px 0;border:1px solid #ddd;border-radius:4px}"
    "button{background:#4CAF50;color:white;padding:10px 15px;border:none;border-radius:4px;cursor:pointer}"
    "</style></head><body>"
    "<h2>BITAXE LED Configuration</h2>"
    "<form action='/save' method='post'>"
    "<label>WiFi SSID:</label><br>"
    "<input type='text' name='ssid' required><br>"
    "<label>WiFi Password:</label><br>"
    "<input type='password' name='password' required><br>"
    "<label>WebSocket Server:</label><br>"
    "<input type='text' name='websocket' required><br>"
    "<button type='submit'>Save Configuration</button>"
    "</form></body></html>";

static esp_err_t root_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, HTML_FORM, strlen(HTML_FORM));
    return ESP_OK;
}

static esp_err_t save_post_handler(httpd_req_t *req)
{
    // Check if request is too large
    if (req->content_len > 1024) {
        ESP_LOGE(TAG, "Request too large: %d bytes", req->content_len);
        httpd_resp_send_err(req, HTTPD_431_REQ_HDR_FIELDS_TOO_LARGE, "Request too large");
        return ESP_FAIL;
    }

    char buf[2048];
    int ret, remaining = req->content_len;

    while (remaining > 0) {
        if ((ret = httpd_req_recv(req, buf, min(remaining, sizeof(buf)))) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            ESP_LOGE(TAG, "Error receiving request data");
            return ESP_FAIL;
        }
        remaining -= ret;
    }
    buf[req->content_len] = '\0';

    // Parse form data
    char *ssid = strstr(buf, "ssid=");
    char *password = strstr(buf, "password=");
    char *websocket = strstr(buf, "websocket=");

    if (ssid && password && websocket) {
        ssid += 5;
        password += 9;
        websocket += 10;

        char *end;
        end = strchr(ssid, '&');
        if (end) *end = '\0';
        end = strchr(password, '&');
        if (end) *end = '\0';
        end = strchr(websocket, '&');
        if (end) *end = '\0';

        strncpy(current_config.wifi_ssid, ssid, sizeof(current_config.wifi_ssid) - 1);
        strncpy(current_config.wifi_password, password, sizeof(current_config.wifi_password) - 1);
        strncpy(current_config.websocket_server, websocket, sizeof(current_config.websocket_server) - 1);

        config_manager_save(&current_config);
        
        const char *response = "<html><body><h2>Configuration saved! Device will restart.</h2></body></html>";
        httpd_resp_send(req, response, strlen(response));
        
        // Restart after a short delay
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_restart();
    }

    return ESP_OK;
}

static httpd_uri_t root = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = root_get_handler,
    .user_ctx = NULL
};

static httpd_uri_t save = {
    .uri = "/save",
    .method = HTTP_POST,
    .handler = save_post_handler,
    .user_ctx = NULL
};

#define BUTTON_GPIO GPIO_NUM_0  // Boot button on most ESP32 boards
#define LONG_PRESS_TIME_MS 300  // 3 seconds for long press

static TimerHandle_t button_timer = NULL;
static bool button_pressed = false;

static void button_timer_callback(TimerHandle_t xTimer)
{
    if (button_pressed) {
        ESP_LOGI(TAG, "Long press detected, clearing configuration");
        nvs_handle_t handle;
        ESP_ERROR_CHECK(nvs_open(CONFIG_NAMESPACE, NVS_READWRITE, &handle));
        ESP_ERROR_CHECK(nvs_erase_all(handle));
        ESP_ERROR_CHECK(nvs_commit(handle));
        nvs_close(handle);
        ESP_LOGI(TAG, "Configuration cleared, restarting...");
        esp_restart();
    }
}

static void IRAM_ATTR button_isr_handler(void* arg)
{
    if (gpio_get_level(BUTTON_GPIO) == 0) {  // Button pressed (active low)
        button_pressed = true;
        xTimerStart(button_timer, 0);
    } else {  // Button released
        button_pressed = false;
        xTimerStop(button_timer, 0);
    }
}

void config_manager_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Setup button for long-press detection
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE
    };
    gpio_config(&io_conf);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_GPIO, button_isr_handler, NULL);

    // Create timer for long-press detection
    button_timer = xTimerCreate("button_timer", pdMS_TO_TICKS(LONG_PRESS_TIME_MS), pdFALSE, NULL, button_timer_callback);

    if (!config_manager_is_configured()) {
        config_manager_start_ap();
    }
}

bool config_manager_load(device_config_t *config)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(CONFIG_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) return false;

    size_t required_size = sizeof(device_config_t);
    err = nvs_get_blob(handle, CONFIG_KEY, config, &required_size);
    nvs_close(handle);

    return (err == ESP_OK);
}

void config_manager_save(const device_config_t *config)
{
    nvs_handle_t handle;
    ESP_ERROR_CHECK(nvs_open(CONFIG_NAMESPACE, NVS_READWRITE, &handle));
    ESP_ERROR_CHECK(nvs_set_blob(handle, CONFIG_KEY, config, sizeof(device_config_t)));
    ESP_ERROR_CHECK(nvs_commit(handle));
    nvs_close(handle);
}

void config_manager_start_ap(void)
{
    ESP_LOGI(TAG, "Starting configuration AP");

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
    assert(ap_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .ap = {
            .max_connection = 4,
            .authmode = WIFI_AUTH_OPEN,
            .ssid_hidden = 0,
            .channel = 1,
        },
    };
    strcpy((char *)wifi_config.ap.ssid, "BITAXE_6PACK");

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    httpd_config_t server_config = HTTPD_DEFAULT_CONFIG();
    server_config.lru_purge_enable = true;
    server_config.max_uri_handlers = 8;
    server_config.max_resp_headers = 8;
    server_config.stack_size = 16384;
    server_config.recv_wait_timeout = 20;
    server_config.send_wait_timeout = 20;
    server_config.max_open_sockets = 4;
    server_config.backlog_conn = 5;

    if (httpd_start(&server, &server_config) == ESP_OK) {
        httpd_register_uri_handler(server, &root);
        httpd_register_uri_handler(server, &save);
        ESP_LOGI(TAG, "HTTP server started");
    } else {
        ESP_LOGE(TAG, "Failed to start HTTP server");
    }
}

bool config_manager_is_configured(void)
{
    device_config_t config;
    return config_manager_load(&config);
} 