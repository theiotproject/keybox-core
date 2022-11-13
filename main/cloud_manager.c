#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs.h"
#include "golioth.h"
#include "cloud_manager.h"

#define CLOUD_EV_CONNECT_BIT BIT(0)
/* report string formats */
#define CLOUD_FORM_OPEN(r) "%llu,%c", (uint64_t)(r)->when, (char)(r)->data.open.access + '0'
#define CLOUD_FORM_WIEGAND(r) "%llu,%hhu,%llx", (uint64_t)(r)->when, (r)->data.wiegand.code_len, (r)->data.wiegand.code
#define CLOUD_FORM_TAMPER(r) "%llu", (uint64_t)(r)->when

typedef struct {
		const char *name;
		cloud_event_t event;
} cloud_numeric_rpc_t;

static void cloud_client_cb(golioth_client_t client, golioth_client_event_t event, void* arg);
static golioth_rpc_status_t cloud_numeric_cb(const char* method, const cJSON* params, uint8_t* detail, size_t detail_size, void* callback_arg);
static golioth_rpc_status_t cloud_wiegand_cb(const char* method, const cJSON* params, uint8_t* detail, size_t detail_size, void* callback_arg);
static golioth_status_t cloud_report_exec(report_data_t *report);

static const char *cloud_tag = "cloud";
/* RPCs with a single numeric parameter */
static const cloud_numeric_rpc_t cloud_numeric_rpcs[] = {
		{
				.name = "led",
				.event = CLOUD_EVENT_LED,
		},
		{
				.name = "open",
				.event = CLOUD_EVENT_OPEN,
		},
};
/* report data paths */
static const char *cloud_report_paths[REPORT_KIND_MAX] = {
		"open",
		"wiegand",
		"tamper"
};
/* events generated in this module */
ESP_EVENT_DEFINE_BASE(CLOUD_EVENT);
/* settings storage */
static nvs_handle_t cloud_nvs_handle;
/* reports connection status */
static EventGroupHandle_t cloud_event_group;
/* guards shared resource */
static SemaphoreHandle_t cloud_mutex;
/* communication handle */
static golioth_client_t cloud_client = NULL;
/* application event loop */
static esp_event_loop_handle_t cloud_event_loop;

/* call once, starts cloud service if configured in flash */
void cloud_init(esp_event_loop_handle_t event_loop)
{
	cloud_mutex = xSemaphoreCreateMutex();
	ESP_ERROR_CHECK(cloud_mutex == NULL ? ESP_ERR_NO_MEM : ESP_OK);
	cloud_event_group = xEventGroupCreate();
	ESP_ERROR_CHECK(cloud_event_group == NULL ? ESP_ERR_NO_MEM : ESP_OK);
	ESP_ERROR_CHECK(nvs_open(cloud_tag, NVS_READWRITE, &cloud_nvs_handle));
	cloud_event_loop = event_loop;
	cloud_join(NULL, NULL);
}

/* updates service configuration */
void cloud_join(char *id, char *psk)
{
	static char cloud_id[CLOUD_ID_MAX_LEN];
	static char cloud_psk[CLOUD_PSK_MAX_LEN];
	golioth_client_config_t config = {};
	size_t len = 0;
	size_t i;

	xSemaphoreTake(cloud_mutex, portMAX_DELAY);
	if(cloud_client) /* disconnect first if needed */
	{
		golioth_client_destroy(cloud_client);
		cloud_client = NULL;
	}
	if(id && psk) /* update config */
	{
		strcpy(cloud_id, id);
		ESP_ERROR_CHECK(nvs_set_str(cloud_nvs_handle, "id", id));
		strcpy(cloud_psk, psk);
		ESP_ERROR_CHECK(nvs_set_str(cloud_nvs_handle, "psk", psk));
		ESP_ERROR_CHECK(nvs_commit(cloud_nvs_handle));
	}
	else /* use stored config if set */
	{
		if(nvs_get_str(cloud_nvs_handle, "id", NULL, &len) != ESP_OK)
			goto cloud_join_no_conf;
		if(!len || len >= CLOUD_ID_MAX_LEN)
			goto cloud_join_no_conf;
		if(nvs_get_str(cloud_nvs_handle, "id", cloud_id, &len) != ESP_OK)
			goto cloud_join_no_conf;
		if(nvs_get_str(cloud_nvs_handle, "psk", NULL, &len) != ESP_OK)
			goto cloud_join_no_conf;
		if(!len || len >= CLOUD_PSK_MAX_LEN)
			goto cloud_join_no_conf;
		if(nvs_get_str(cloud_nvs_handle, "psk", cloud_psk, &len) != ESP_OK)
			goto cloud_join_no_conf;
	}
	config.credentials.auth_type = GOLIOTH_TLS_AUTH_TYPE_PSK;
	config.credentials.psk.psk_id = cloud_id;
	config.credentials.psk.psk_id_len = strlen(cloud_id);
	config.credentials.psk.psk = cloud_psk;
	config.credentials.psk.psk_len = strlen(cloud_psk);
	cloud_client = golioth_client_create(&config);
	ESP_ERROR_CHECK(cloud_client == NULL ? ESP_FAIL : ESP_OK);
	golioth_client_register_event_callback(cloud_client, cloud_client_cb, NULL);
	len = sizeof(cloud_numeric_rpcs)/sizeof(cloud_numeric_rpc_t);
	for(i=0; i<len; i++)
		golioth_rpc_register(cloud_client, cloud_numeric_rpcs[i].name, cloud_numeric_cb, NULL);
	golioth_rpc_register(cloud_client, "wiegand", cloud_wiegand_cb, NULL);
	ESP_LOGI(cloud_tag, "Joining as %s", cloud_id);
	xSemaphoreGive(cloud_mutex);
	return;
cloud_join_no_conf:
	ESP_LOGI(cloud_tag, "No configuration");
	xSemaphoreGive(cloud_mutex);
}

/* disconnects from service */
void cloud_leave(void)
{
	xSemaphoreTake(cloud_mutex, portMAX_DELAY);
	if(cloud_client)
	{
		golioth_client_destroy(cloud_client);
		cloud_client = NULL;
		/* erase configuration */
		ESP_ERROR_CHECK(nvs_erase_all(cloud_nvs_handle));
		ESP_ERROR_CHECK(nvs_commit(cloud_nvs_handle));
		/* substitute disconnect event */
		ESP_LOGI(cloud_tag, "Disconnected");
		ESP_ERROR_CHECK(esp_event_post_to(cloud_event_loop, CLOUD_EVENT, CLOUD_EVENT_DISCONNECTED, NULL, 0, portMAX_DELAY));
	}
	xSemaphoreGive(cloud_mutex);
}

/* logs a message to cloud */
void cloud_log(const char *tag, const char *format, ...)
{
	static char cloud_log_buf[CLOUD_LOG_MAX_LEN];
	va_list list;
	int ret;

	xSemaphoreTake(cloud_mutex, portMAX_DELAY);
	if(cloud_client)
	{
		va_start(list, format);
		ret = vsnprintf(cloud_log_buf, CLOUD_LOG_MAX_LEN, format, list);
		if(ret < CLOUD_LOG_MAX_LEN)
		{
			ESP_LOGD(cloud_tag, "Sending log: %s", cloud_log_buf);
			golioth_log_info_async(cloud_client, tag, cloud_log_buf, NULL, NULL);
		}
		else
		{
			ESP_LOGE(cloud_tag, "Oversized log");
		}
		va_end(list);
	}
	xSemaphoreGive(cloud_mutex);
}

/* uploads event report to cloud, blocks until completion */
void cloud_report(report_data_t *report)
{
	golioth_status_t ret;

	while(true)
	{
		if(!golioth_client_is_connected(cloud_client)) /* can be called with NULL */
		{
			/* block and wait for connection */
			xEventGroupWaitBits(cloud_event_group, CLOUD_EV_CONNECT_BIT, pdTRUE, pdFALSE, portMAX_DELAY);
		}
		xSemaphoreTake(cloud_mutex, portMAX_DELAY);
		ret = cloud_report_exec(report);
		xSemaphoreGive(cloud_mutex);
		if(ret != GOLIOTH_OK) /* try again */
		{
			vTaskDelay(pdMS_TO_TICKS(1000*CONFIG_CLOUD_RETRY_TIME));
		}
		else /* done */
		{
			break;
		}
	}
}

/* connect/disconnect events */
static void cloud_client_cb(golioth_client_t client, golioth_client_event_t event, void *arg)
{
	(void)client;
	(void)arg;

	switch(event)
	{
	case GOLIOTH_CLIENT_EVENT_CONNECTED:
		ESP_LOGI(cloud_tag, "Connected");
		ESP_ERROR_CHECK(esp_event_post_to(cloud_event_loop, CLOUD_EVENT, CLOUD_EVENT_CONNECTED, NULL, 0, portMAX_DELAY));
		xEventGroupSetBits(cloud_event_group, CLOUD_EV_CONNECT_BIT);
		break;
	case GOLIOTH_CLIENT_EVENT_DISCONNECTED:
		ESP_LOGI(cloud_tag, "Disconnected");
		ESP_ERROR_CHECK(esp_event_post_to(cloud_event_loop, CLOUD_EVENT, CLOUD_EVENT_DISCONNECTED, NULL, 0, portMAX_DELAY));
		break;
	}
}

/* common parser for RPCs with a single numeric parameter and no return data */
static golioth_rpc_status_t cloud_numeric_cb(const char* method, const cJSON* params, uint8_t* detail, size_t detail_size, void* callback_arg)
{
	cJSON *item;
	size_t len;
	size_t i;
	(void)callback_arg;
	(void)detail_size;
	(void)detail;

	if(cJSON_GetArraySize(params) != 1)
	{
		ESP_LOGE(cloud_tag, "Incorrect %s parameter count", method);
		return(RPC_INVALID_ARGUMENT);
	}
	item = cJSON_GetArrayItem(params, 0);
	if(!cJSON_IsNumber(item))
	{
		ESP_LOGE(cloud_tag, "Incorrect %s parameter type", method);
		return(RPC_INVALID_ARGUMENT);
	}
	ESP_LOGI(cloud_tag, "%s %d", method, item->valueint);
	len = sizeof(cloud_numeric_rpcs)/sizeof(cloud_numeric_rpc_t);
	for(i=0; i<len; i++)
		if(!strcmp(method, cloud_numeric_rpcs[i].name))
		{
			ESP_ERROR_CHECK(esp_event_post_to(cloud_event_loop, CLOUD_EVENT, cloud_numeric_rpcs[i].event, &item->valueint, sizeof(item->valueint), portMAX_DELAY));
			return(RPC_OK);
		}
	return(RPC_UNKNOWN);
}

/* parser for Wiegand RPC */
static golioth_rpc_status_t cloud_wiegand_cb(const char* method, const cJSON* params, uint8_t* detail, size_t detail_size, void* callback_arg)
{
	cJSON *item;
	cloud_wiegand_data_t wiegand_data;
	(void)method;
	(void)callback_arg;
	(void)detail_size;
	(void)detail;

	if(cJSON_GetArraySize(params) != 2)
	{
		ESP_LOGE(cloud_tag, "Incorrect wiegand parameter count");
		return(RPC_INVALID_ARGUMENT);
	}
	item = cJSON_GetArrayItem(params, 0);
	if(!cJSON_IsNumber(item))
	{
		ESP_LOGE(cloud_tag, "Incorrect wiegand length type");
		return(RPC_INVALID_ARGUMENT);
	}
	if(item->valueint <= 0 || item->valueint > 63)
	{
		ESP_LOGE(cloud_tag, "Incorrect wiegand length %d", item->valueint);
		return(RPC_INVALID_ARGUMENT);
	}
	wiegand_data.code_len = item->valueint;
	item = cJSON_GetArrayItem(params, 1);
	if(!cJSON_IsString(item))
	{
		ESP_LOGE(cloud_tag, "Incorrect wiegand code type");
		return(RPC_INVALID_ARGUMENT);
	}
	if(sscanf(item->valuestring, "%llx", &wiegand_data.code) != 1)
	{
		ESP_LOGE(cloud_tag, "Incorrect wiegand code content %s", item->valuestring);
		return(RPC_INVALID_ARGUMENT);
	}
	ESP_ERROR_CHECK(esp_event_post_to(cloud_event_loop, CLOUD_EVENT, CLOUD_EVENT_WIEGAND, &wiegand_data, sizeof(wiegand_data), portMAX_DELAY));
	return(RPC_OK);
}

/* formats and uploads report to cloud */
static golioth_status_t cloud_report_exec(report_data_t *report)
{
	char *buf;
	golioth_status_t ret;
	size_t len;

	switch(report->kind)
	{
	case REPORT_KIND_OPEN:
		len = snprintf(NULL, 0, CLOUD_FORM_OPEN(report));
		buf = malloc(len + 1);
		sprintf(buf, CLOUD_FORM_OPEN(report));
		break;
	case REPORT_KIND_WIEGAND:
		len = snprintf(NULL, 0, CLOUD_FORM_WIEGAND(report));
		buf = malloc(len + 1);
		sprintf(buf, CLOUD_FORM_WIEGAND(report));
		break;
	case REPORT_KIND_TAMPER:
		len = snprintf(NULL, 0, CLOUD_FORM_TAMPER(report));
		buf = malloc(len + 1);
		sprintf(buf, CLOUD_FORM_TAMPER(report));
		break;
	default:
		ESP_LOGW(cloud_tag, "Skipped unsupported report kind %u", (uint32_t)report->kind);
		return(GOLIOTH_OK);
	}
	ESP_LOGD(cloud_tag, "Path: %s, report: %s", cloud_report_paths[report->kind], buf);
	ret = golioth_lightdb_stream_set_string_sync(cloud_client, cloud_report_paths[report->kind], buf, len, GOLIOTH_WAIT_FOREVER); /* waits for time specified in the Golioth configuration */
	free(buf);
	return(ret);
}
