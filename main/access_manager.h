#ifndef KEY_SCANNER_ESP32_ACCESS_MANAGER_H
#define KEY_SCANNER_ESP32_ACCESS_MANAGER_H


void access_init();
void access_set_magic(char *field);
void access_check_magic(char *field);

#endif //KEY_SCANNER_ESP32_ACCESS_MANAGER_H