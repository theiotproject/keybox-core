#include "report_manager.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "ui_manager.h"
#include "string.h"

#ifndef KEY_SCANNER_ESP32_ACCESS_MANAGER_H
#define KEY_SCANNER_ESP32_ACCESS_MANAGER_H

void set_magic(char *field);
void access_init(char *magic);
void check_magic(char *field, char *magic);

#endif //KEY_SCANNER_ESP32_ACCESS_MANAGER_H