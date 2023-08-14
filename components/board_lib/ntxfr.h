#ifndef NTXFR_H
#define NTXFR_H

#include <glob.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    const uint8_t *ptr;
    size_t len;
} ntxfr_data_t;

uint8_t ntxfr_get_addr(const ntxfr_data_t payload);
uint8_t ntxfr_get_cmd(const ntxfr_data_t payload);
inline uint8_t ntxfr_get_res(const ntxfr_data_t payload){ return ntxfr_get_cmd(payload); };
ntxfr_data_t ntxfr_get_data(const ntxfr_data_t payload);
bool ntxfr_is_valid(const ntxfr_data_t payload);

#endif //NTXFR_H
