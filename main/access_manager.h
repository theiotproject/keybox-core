#ifndef KEY_SCANNER_ESP32_ACCESS_MANAGER_H
#define KEY_SCANNER_ESP32_ACCESS_MANAGER_H

#include <stdbool.h>

// 0..4 (card id), 5 (slots) 
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

#endif //KEY_SCANNER_ESP32_ACCESS_MANAGER_H