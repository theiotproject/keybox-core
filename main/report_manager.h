#ifndef MAIN_REPORT_MANAGER_H_
#define MAIN_REPORT_MANAGER_H_

#include <time.h>
#include "esp_partition.h"

/* maximum card id length */
#define REPORT_MAX_CARD_ID_LEN 10

/* report content types */
typedef enum {
	REPORT_KIND_OPEN,
	REPORT_KIND_WIEGAND,
	REPORT_KIND_TAMPER,
	REPORT_KIND_BUTTON,
	REPORT_KIND_MAGIC,
    REPORT_KIND_NEWCARD,
	REPORT_KIND_MAX
} report_kind_t;

/* report data structure */
typedef struct
{
	report_kind_t kind;
	time_t when;
	union {
		struct {
			bool access;
		} open;
		struct {
			uint64_t code;
			uint8_t code_len;
		} wiegand;
		struct {
			bool access;
		} magic;
		struct {
			char id[REPORT_MAX_CARD_ID_LEN+1];
			uint8_t id_len;
		} card;
	} data;
} report_data_t;

void report_start(const esp_partition_t *partition);
void report_add(report_data_t *data);

#endif /* MAIN_REPORT_MANAGER_H_ */
