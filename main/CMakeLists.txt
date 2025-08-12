idf_component_register(
    SRCS "main.c"
    "lights.c"
    "wifi_manager.c"
    "websocket.c"
    "config_manager.c"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash esp_websocket_client esp_wifi esp_http_client 
        esp_event esp_netif json driver neopixel esp_http_server
)