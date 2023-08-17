#include <string.h>
#include <stdio.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "task_prio.h"
#include "board_lib.h"
#include "flash_ring.h"
#include "wifi_manager.h"
#include "ui_manager.h"
#include "cloud_manager.h"
#include "report_manager.h"

static const char *app_tag = "app";

static void app_event_cb(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

const esp_partition_t *app_fring_partition;
static esp_event_loop_handle_t app_event_loop;

void app_main(void)
{
	esp_err_t ret;

	/* main application event loop */
	const esp_event_loop_args_t loop_args = {
			.queue_size = 16,
			.task_name = "app_ev_loop",
			.task_priority = TP_MAIN,
			.task_stack_size = 4096 + configMINIMAL_STACK_SIZE,
			.task_core_id = tskNO_AFFINITY
	};
	/* application events */
	ESP_ERROR_CHECK(esp_event_loop_create(&loop_args, &app_event_loop));
	ESP_ERROR_CHECK(esp_event_handler_instance_register_with(app_event_loop, ESP_EVENT_ANY_BASE, ESP_EVENT_ANY_ID, app_event_cb, NULL, NULL));
	/* system events */
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, app_event_cb, NULL, NULL));
	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, app_event_cb, NULL, NULL));
	/* NVS for settings storage */
	ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_LOGI(app_tag, "NVS init erase");
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);
	/* storage for produced reports */
	app_fring_partition = esp_partition_find_first(0x40, 0x00, "flash_ring");
	board_init(app_event_loop); /* all low level inits */
	ui_start();
	wifi_init(); /* connects to network if configured in the NVS */
	report_start(app_fring_partition); /* saves and uploads reports */
	board_reader_start(app_event_loop, TP_READER); /* reads QR codes */
	cloud_init(app_event_loop); /* connects to cloud if configured in the NVS */
}

/* executed by the application loop task */
static void app_event_cb(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
	static bool app_wifi_connected = false;
	static bool app_cloud_connected = false;
	char *reader_info = NULL;
	uint32_t led_brg;
	struct timeval sys_time;
	char *time_str;
	report_data_t report_data;
	(void)event_handler_arg;

	if(event_base == BOARD_EVENT)
	{
		switch(event_id)
		{
		case BOARD_EVENT_NEW_CARD: /* process code */
		{
			uint64_t *id = event_data;
			ESP_LOGD(app_tag, "Received card ID: %llu", *id);
			break;
			}
		case BOARD_EVENT_BUTTON:
            {
            uint8_t* button = event_data;
            report_data.when = 0;
			report_data.kind = REPORT_KIND_NEW_CARD;
			report_data.data.card_id = 123456789;
			report_add(&report_data);
			ui_rg_beep_open(UI_ACCESS_GRANTED);
            ESP_LOGD(app_tag, "button pressed: %d", *button);
            break;
            }
		}
		return;
	}
	if(event_base == IP_EVENT) /* only IP_EVENT_STA_GOT_IP */
	{
		app_wifi_connected = true;
		ui_blue(app_cloud_connected ? UI_NET_OK : UI_NET_NO_CLOUD);
		if(app_cloud_connected && reader_info)
		{
			cloud_log(app_tag, reader_info);
			free(reader_info);
			reader_info = NULL;
		}
		return;
	}
	if(event_base == WIFI_EVENT) /* only WIFI_EVENT_STA_DISCONNECTED */
	{
		if(app_wifi_connected)
		{
			ui_blue(UI_NET_NO_WIFI);
			app_wifi_connected = false;
		}
		return;
	}
	if(event_base == CLOUD_EVENT)
	{
		switch(event_id)
		{
			case CLOUD_EVENT_CONNECTED:
				app_cloud_connected = true;
				ui_blue(app_wifi_connected ? UI_NET_OK : UI_NET_NO_WIFI);
				gettimeofday(&sys_time, NULL);
				time_str = ctime(&sys_time.tv_sec);
				time_str[strlen(time_str) - 1] = 0;
				cloud_log(app_tag, "HWt " BOARD_HW_INFO " %s", time_str);
				if(app_wifi_connected && reader_info)
				{
					cloud_log(app_tag, reader_info);
					free(reader_info);
					reader_info = NULL;
				}
				break;
			case CLOUD_EVENT_DISCONNECTED:
				app_cloud_connected = false;
				ui_blue(app_wifi_connected ? UI_NET_NO_CLOUD : UI_NET_NO_WIFI);
				break;
			case CLOUD_EVENT_LED: /* RPC led, parameter: number 0 - 256 */
				led_brg = *(int *)event_data;
				if(led_brg > UI_MAX_LED_BRG)
					led_brg = UI_MAX_LED_BRG;
				ui_brightness(led_brg);
				break;
			case CLOUD_EVENT_OPEN: /* RPC parameter: number 1 - grant access, other - deny */
				if(*(int *)event_data == 1)
				{
					ui_rg_beep_open(UI_ACCESS_GRANTED);
				}
				else
				{
					ui_rg_beep_open(UI_ACCESS_DENIED);
				}
				break;
		}
		return;
	}
}
