#ifndef KEY_SCANNER_ESP32_ACCESS_MANAGER_H
#define KEY_SCANNER_ESP32_ACCESS_MANAGER_H

#include <stdbool.h>

void access_init();
void access_set_magic(char *field);
bool access_check_magic(char *field);
bool access_process_code_open(char *data);

#endif //KEY_SCANNER_ESP32_ACCESS_MANAGER_H
