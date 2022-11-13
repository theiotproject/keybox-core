#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "task_prio.h"
#include "flash_ring.h"
#include "rtc_daemon.h"
#include "cloud_manager.h"

static const char *report_tag = "report";

static void report_upload_task(void *arg);
static void report_tamper_task(void *arg);

/* report data storage */
static fring_context_t *report_fring_ctx;

/* starts upload task */
void report_start(const esp_partition_t *partition)
{
	BaseType_t ret;

	report_fring_ctx = fring_init(partition);
	ESP_ERROR_CHECK(report_fring_ctx == NULL ? ESP_ERR_NO_MEM : ESP_OK);
	ret = xTaskCreate(report_upload_task, report_tag, 2048 + configMINIMAL_STACK_SIZE, NULL, TP_UPLOAD, NULL);
	ESP_ERROR_CHECK(ret != pdPASS ? ESP_ERR_NO_MEM : ESP_OK);
	ret = xTaskCreate(report_tamper_task, "rep-tamp", 2048 + configMINIMAL_STACK_SIZE, NULL, TP_TAMPER, NULL);
	ESP_ERROR_CHECK(ret != pdPASS ? ESP_ERR_NO_MEM : ESP_OK);
}

/* stores report data in flash, blocks until completion */
void report_add(report_data_t *data)
{
	struct timeval sys_time;

	/* add time if not set */
	if(!data->when)
	{
		gettimeofday(&sys_time, NULL);
		data->when = sys_time.tv_sec;
	}
	fring_write(report_fring_ctx, data, sizeof(report_data_t));
}

/* uploads reports to cloud */
static void report_upload_task(void *arg)
{
	report_data_t data;
	size_t data_size = 0;
	(void)arg;

	while(true)
	{
		fring_read(report_fring_ctx, &data, &data_size, portMAX_DELAY); /* block task until new data arrives */
		cloud_report(&data); /* block task until upload completes */
		fring_confirm_read(report_fring_ctx);
	}
}

/* waits for tamper events */
static void report_tamper_task(void *arg)
{
	report_data_t data;
	(void)arg;

	data.kind = REPORT_KIND_TAMPER;
	while(true)
	{
		rtcd_fetch_tamper(&data.when, portMAX_DELAY); /* block task until detected */
		report_add(&data);
	}
}
