#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <stdbool.h>

typedef struct {
    char wifi_ssid[32];
    char wifi_password[64];
    char websocket_server[128];
} device_config_t;

void config_manager_init(void);
bool config_manager_load(device_config_t *config);
void config_manager_save(const device_config_t *config);
void config_manager_start_ap(void);
bool config_manager_is_configured(void);

#endif // CONFIG_MANAGER_H 