#ifndef MAIN_REPORT_MANAGER_H_
#define MAIN_REPORT_MANAGER_H_

#include <time.h>
#include "esp_partition.h"

/* report content types */
typedef enum {
	REPORT_KIND_NEW_CARD,
	REPORT_KIND_MAX
} report_kind_t;

/* report data structure */
typedef struct
{
	report_kind_t kind;
	time_t when;
	union {
			uint64_t card_id;
	} data;
} report_data_t;

void report_start(const esp_partition_t *partition);
void report_add(report_data_t *data);

#endif /* MAIN_REPORT_MANAGER_H_ */
