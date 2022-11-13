#ifndef MAIN_REPORT_MANAGER_H_
#define MAIN_REPORT_MANAGER_H_

#include <time.h>
#include "esp_partition.h"

/* report content types */
typedef enum {
	REPORT_KIND_OPEN,
	REPORT_KIND_WIEGAND,
	REPORT_KIND_TAMPER,
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
	} data;
} report_data_t;

void report_start(const esp_partition_t *partition);
void report_add(report_data_t *data);

#endif /* MAIN_REPORT_MANAGER_H_ */
