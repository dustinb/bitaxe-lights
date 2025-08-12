#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <stdbool.h>

#define MAX_SSID_LENGTH 32
#define MAX_PASSWORD_LENGTH 64
#define MAX_WEBSOCKET_LENGTH 128

typedef struct {
    char wifi_ssid[MAX_SSID_LENGTH];
    char wifi_password[MAX_PASSWORD_LENGTH];
    char websocket_server[MAX_WEBSOCKET_LENGTH];
} device_config_t;

void config_manager_init(void);
bool config_manager_load(device_config_t *config);
void config_manager_save(const device_config_t *config);
void config_manager_start_ap(void);
bool config_manager_is_configured(void);

#endif // CONFIG_MANAGER_H 