#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "nvs.h"
#include "wifi_manager.h"

/* event group bits */
#define WIFI_EV_STOP_BIT BIT0

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

static const char *wifi_tag = "wifi";

/* settings storage */
static nvs_handle_t wifi_nvs_handle;
/* reports connection status */
static EventGroupHandle_t wifi_event_group;
/* controls re-connect */
static bool wifi_activated;
/* guards shared resource */
static SemaphoreHandle_t wifi_mutex;

/* inits stack, joins network if configured */
void wifi_init(void)
{
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

	wifi_mutex = xSemaphoreCreateMutex();
	ESP_ERROR_CHECK(wifi_mutex == NULL ? ESP_ERR_NO_MEM : ESP_OK);
	wifi_event_group = xEventGroupCreate();
	ESP_ERROR_CHECK(wifi_event_group == NULL ? ESP_ERR_NO_MEM : ESP_OK);
	ESP_ERROR_CHECK(esp_netif_init());
	esp_netif_create_default_wifi_sta();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));
	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, NULL));
	ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, NULL));
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(nvs_open(wifi_tag, NVS_READWRITE, &wifi_nvs_handle));
	wifi_join(CONFIG_WIFI_SSID, CONFIG_WIFI_PASS);
}

/* sets configuration and joins network, also used to change network on the fly */
void wifi_join(const char *ssid, const char *pass)
{
	wifi_config_t config = {};
	size_t len = 0;

	xSemaphoreTake(wifi_mutex, portMAX_DELAY);
	if(wifi_activated) /* disconnect first if needed */
	{
		wifi_activated = false;
		ESP_ERROR_CHECK(esp_wifi_stop());
		/* wait for stop event */
		xEventGroupWaitBits(wifi_event_group, WIFI_EV_STOP_BIT, pdTRUE, pdFALSE, portMAX_DELAY);
	}
	if(ssid && pass) /* update config */
	{
		strcpy((char *)config.sta.ssid, ssid);
		ESP_ERROR_CHECK(nvs_set_str(wifi_nvs_handle, "ssid", ssid));
		strcpy((char *)config.sta.password, pass);
		ESP_ERROR_CHECK(nvs_set_str(wifi_nvs_handle, "pass", pass));
		ESP_ERROR_CHECK(nvs_commit(wifi_nvs_handle));
	}
	else /* use stored config if set */
	{
		if(nvs_get_str(wifi_nvs_handle, "ssid", NULL, &len) != ESP_OK)
			goto wifi_join_no_conf;
		if(!len || len >= WIFI_SSID_MAX_LEN)
			goto wifi_join_no_conf;
		if(nvs_get_str(wifi_nvs_handle, "ssid", (char *)config.sta.ssid, &len) != ESP_OK)
			goto wifi_join_no_conf;
		if(nvs_get_str(wifi_nvs_handle, "pass", NULL, &len) != ESP_OK)
			goto wifi_join_no_conf;
		if(len < WIFI_PASS_MIN_LEN || len >= WIFI_PASS_MAX_LEN)
			goto wifi_join_no_conf;
		if(nvs_get_str(wifi_nvs_handle, "pass", (char *)config.sta.password, &len) != ESP_OK)
			goto wifi_join_no_conf;
	}
	config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &config));
	ESP_LOGI(wifi_tag, "Joining %s", config.sta.ssid);
	wifi_activated = true;
	ESP_ERROR_CHECK(esp_wifi_start());
	xSemaphoreGive(wifi_mutex);
	return;
wifi_join_no_conf:
	ESP_LOGI(wifi_tag, "No configuration");
	xSemaphoreGive(wifi_mutex);
}

/* disconnects network */
void wifi_leave(void)
{
	xSemaphoreTake(wifi_mutex, portMAX_DELAY);
	/* disconnect if needed */
	if(wifi_activated)
	{
		wifi_activated = false;
		esp_wifi_stop();
		/* wait for stop event */
		xEventGroupWaitBits(wifi_event_group, WIFI_EV_STOP_BIT, pdTRUE, pdFALSE, portMAX_DELAY);
	}
	/* erase configuration */
	ESP_ERROR_CHECK(nvs_erase_all(wifi_nvs_handle));
	ESP_ERROR_CHECK(nvs_commit(wifi_nvs_handle));
	xSemaphoreGive(wifi_mutex);
}

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
	ip_event_got_ip_t *event;

	if(event_base == WIFI_EVENT)
	{
		switch(event_id)
		{
		case WIFI_EVENT_STA_START:
			ESP_LOGD(wifi_tag, "Started");
			ESP_ERROR_CHECK(esp_wifi_connect()); /* 1st connect */
			break;
		case WIFI_EVENT_STA_DISCONNECTED:
			ESP_LOGI(wifi_tag, "Disconnected");
			if(wifi_activated) /* unwanted disconnect */
				ESP_ERROR_CHECK(esp_wifi_connect()); /* try to reconnect */
			break;
		case WIFI_EVENT_STA_STOP:
			ESP_LOGD(wifi_tag, "Stopped");
			xEventGroupSetBits(wifi_event_group, WIFI_EV_STOP_BIT);
			break;
		}
	}
	else if(event_base == IP_EVENT)
	{
		switch(event_id)
		{
		case IP_EVENT_STA_GOT_IP:
			event = (ip_event_got_ip_t *)event_data;
			ESP_LOGI(wifi_tag, "Got ip:" IPSTR, IP2STR(&event->ip_info.ip));
			break;
		}
	}
}
