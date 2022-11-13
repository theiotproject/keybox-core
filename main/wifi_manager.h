#ifndef MAIN_WIFI_MANAGER_H_
#define MAIN_WIFI_MANAGER_H_

#include "esp_wifi.h"

#define WIFI_SSID_MAX_LEN (sizeof(((wifi_config_t *)0)->sta.ssid))
#define WIFI_PASS_MIN_LEN 8
#define WIFI_PASS_MAX_LEN (sizeof(((wifi_config_t *)0)->sta.password))

void wifi_init(void);
void wifi_join(const char *ssid, const char *pass);
void wifi_leave(void);

#endif /* MAIN_WIFI_MANAGER_H_ */
