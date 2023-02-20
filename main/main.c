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
#include "rtc_daemon.h"
#include "flash_ring.h"
#include "wifi_manager.h"
#include "ui_manager.h"
#include "cloud_manager.h"
#include "report_manager.h"
#include "access_manager.h"
#include "mecfmt.h"

static const char *app_tag = "app";

static void app_event_cb(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

const esp_partition_t *app_fring_partition;
static esp_event_loop_handle_t app_event_loop;

void app_main(void)
{
	esp_err_t ret;
	rtcd_config_t rtcd_conf = {
			.enable_ntp_from_dhcp = true,
			.static_server_names[0] = "pool.ntp.org",
			.static_server_names[1] = NULL,
			.task_priority = TP_RTCD,
	};
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
	rtcd_start(&rtcd_conf); /* acquires time */
	report_start(app_fring_partition); /* saves and uploads reports */
	board_reader_start(app_event_loop, TP_READER); /* reads QR codes */
	cloud_init(app_event_loop); /* connects to cloud if configured in the NVS */
	access_init();
}

/* executed by the application loop task */
static void app_event_cb(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
	static bool app_wifi_connected = false;
	static bool app_cloud_connected = false;
	char *reader_info = NULL;
	char *field;
	uint32_t led_brg;
	size_t field_len;
	char *wifi_ssid;
	char *cloud_id;
	uint8_t wiegand_len;
	uint64_t wiegand_frame;
	struct timeval sys_time;
	char *time_str;
	report_data_t report_data;
	(void)event_handler_arg;

	if(event_base == BOARD_EVENT)
	{
		switch(event_id)
		{
		case BOARD_EVENT_READER_READY: /* log reader info to cloud */
			if(app_wifi_connected && app_cloud_connected) /* do it now */
			{
				cloud_log(app_tag, event_data);
			}
			else /* do it later */
			{
				reader_info = malloc(strlen(event_data) + 1);
				ESP_ERROR_CHECK(reader_info == NULL ? ESP_ERR_NO_MEM : ESP_OK);
				strcpy(reader_info, event_data);
			}
			break;
		case BOARD_EVENT_NEW_CODE: ;/* process code */
			char *ev_data = (char*) event_data;
			char *data_to_parse = strdup(ev_data);
			if (!data_to_parse)
			{
				return;
			}
			field = strtok(event_data, ",");
			if(strlen(ev_data) == strlen(data_to_parse))
			{
				field = strtok(event_data, ":");
				if(field && !strcmp(field, "OPEN"))
				{
					char valueString[128];
					mecfmt_value_t value;
					const char *keys[] = {"ID", "VF", "VT", "L", "S"};
					for (unsigned int i = 0; i < sizeof(keys) / sizeof(char *); i++) {
						value = mecfmt_get_velue(data_to_parse, keys[i]);
						mecfmt_value_to_string(&value, valueString);
						ESP_LOGI(app_tag, "Got key value %s", valueString);
					}

					free(data_to_parse);
					return;
				}
				free(data_to_parse);
			}
			if(!strcmp(field, "factory")) /* example: factory,reset */
			{
				field = strtok(NULL, ",");
				if(!field)
					return;
				if(strcmp(field, "reset"))
					return;
				ESP_ERROR_CHECK(esp_partition_erase_range(app_fring_partition, 0, app_fring_partition->size));
				ESP_ERROR_CHECK(nvs_flash_erase());
				esp_restart();
			}
			if(!strcmp(field, "wifi")) /* examples: wifi,1,ssid,password wifi,0 */
			{
				field = strtok(NULL, ",");
				if(!field)
					return;
				if(field[0] != '1')
				{
					wifi_leave();
					ui_rg_beep_open(UI_SET_WIFI);
					return;
				}
				field = strtok(NULL, ",");
				if(!field)
					return;
				field_len = strnlen(field, WIFI_SSID_MAX_LEN);
				if(!field_len || field_len == WIFI_SSID_MAX_LEN)
					return;
				wifi_ssid = malloc(field_len + 1);
				ESP_ERROR_CHECK(wifi_ssid == NULL ? ESP_ERR_NO_MEM : ESP_OK);
				strcpy(wifi_ssid, field);
				field = strtok(NULL, ",");
				if(!field)
				{
					free(wifi_ssid);
					return;
				}
				field_len = strnlen(field, WIFI_PASS_MAX_LEN);
				if(field_len < WIFI_PASS_MIN_LEN || field_len == WIFI_PASS_MAX_LEN)
				{
					free(wifi_ssid);
					return;
				}
				wifi_join(wifi_ssid, field);
				free(wifi_ssid);
				ui_rg_beep_open(UI_SET_WIFI);
				return;
			}
			if(!strcmp(field, "cloud")) /* examples: cloud,1,id,psk cloud,0 */
			{
				field = strtok(NULL, ",");
				if(!field)
					return;
				if(field[0] != '1')
				{
					cloud_leave();
					ui_rg_beep_open(UI_SET_CLOUD);
					return;
				}
				field = strtok(NULL, ",");
				if(!field)
					return;
				field_len = strnlen(field, CLOUD_ID_MAX_LEN);
				if(!field_len || field_len == CLOUD_ID_MAX_LEN)
					return;
				cloud_id = malloc(field_len + 1);
				ESP_ERROR_CHECK(cloud_id == NULL ? ESP_ERR_NO_MEM : ESP_OK);
				strcpy(cloud_id, field);
				field = strtok(NULL, ",");
				if(!field)
				{
					free(cloud_id);
					return;
				}
				field_len = strnlen(field, CLOUD_PSK_MAX_LEN);
				if(!field_len || field_len == CLOUD_PSK_MAX_LEN)
				{
					free(cloud_id);
					return;
				}
				cloud_join(cloud_id, field);
				free(cloud_id);
				ui_rg_beep_open(UI_SET_CLOUD);
				return;
			}
			if(!strcmp(field, "conf"))
			{
				field = strtok(NULL, ",");
				access_set_magic(field);
			}
			if(!strcmp(field, "magic"))
			{
				field = strtok(NULL, ",");
				access_check_magic(field);
			}
			if(!strcmp(field, "led")) /* example: led,48 */
			{
				field = strtok(NULL, ",");
				if(!field)
					return;
				if(sscanf(field, "%u", &led_brg) != 1)
					return;
				if(led_brg > UI_MAX_LED_BRG)
					return;
				ui_brightness(led_brg);
				ui_rg_beep_open(UI_SET_LED);
				return;
			}
			if(!strcmp(field, "open")) /* example: open,1 */
			{
				field = strtok(NULL, ",");
				if(!field)
					return;
				report_data.when = 0;
				report_data.kind = REPORT_KIND_OPEN;
				if(field[0] == '1')
				{
					report_data.data.open.access = true;
					report_add(&report_data);
					ui_rg_beep_open(UI_ACCESS_GRANTED);
				}
				else
				{
					report_data.data.open.access = false;
					report_add(&report_data);
					ui_rg_beep_open(UI_ACCESS_DENIED);
				}
				return;
			}
			if(!strcmp(field, "wiegand")) /* example: wiegand,26,2abcde1 */
			{
				field = strtok(NULL, ",");
				if(!field)
					return;
				if(sscanf(field, "%hhu", &wiegand_len) != 1)
					return;
				if(!wiegand_len || wiegand_len > 63)
					return;
				field = strtok(NULL, ",");
				if(!field)
					return;
				if(sscanf(field, "%llx", &wiegand_frame) != 1)
					return;
				report_data.when = 0;
				report_data.kind = REPORT_KIND_WIEGAND;
				report_data.data.wiegand.code = wiegand_frame;
				report_data.data.wiegand.code_len = wiegand_len;
				report_add(&report_data);
				ui_rg_beep_open(UI_WIEGAND);
				board_wiegand_send(wiegand_frame, wiegand_len);
			}
			break;
		case BOARD_EVENT_BUTTON:
			report_data.when = 0;
			report_data.kind = REPORT_KIND_BUTTON;
			report_add(&report_data);
			ui_rg_beep_open(UI_ACCESS_GRANTED);
			break;
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
				cloud_log(app_tag, "HWt " BOARD_HW_INFO " %umV %s", board_supply_meas(), time_str);
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
			case CLOUD_EVENT_WIEGAND: /* RPC parameters: number 1 - 63 - bit count, hex string - data (sent MSB first) */
				ui_rg_beep_open(UI_WIEGAND);
				board_wiegand_send(((cloud_wiegand_data_t *)event_data)->code, ((cloud_wiegand_data_t *)event_data)->code_len);
				break;
		}
		return;
	}
}
