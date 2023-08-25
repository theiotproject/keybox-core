#ifndef KEY_SCANNER_ESP32_ACCESS_MANAGER_H
#define KEY_SCANNER_ESP32_ACCESS_MANAGER_H

#include <stdbool.h>
#include <stdint.h>
#include "nvs.h"

typedef union {
    uint8_t data[6];
} ac_t;

typedef enum {
    CARD_ID_BYTE_0,
    CARD_ID_BYTE_1,
    CARD_ID_BYTE_2,
    CARD_ID_BYTE_3,
    CARD_ID_BYTE_4,
    SLOTS_BYTE
} acl_byte_map;

void access_init();
bool access_find_card_id_in_nvs(uint64_t card_id, uint8_t *privilege_to_slots);
void access_save_card_id_in_nvs(uint64_t card_id, uint8_t privilege_to_slots);
void access_fill_with_zeros_acl(void);
esp_err_t access_get_acl_from_nvs(void);
esp_err_t access_set_acl_in_nvs(void);

#endif //KEY_SCANNER_ESP32_ACCESS_MANAGER_H