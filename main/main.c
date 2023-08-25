#include <string.h>
#include <stdio.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
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
#include "access_manager.h"

static const char *app_tag = "app";

static void app_event_cb(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void servo_close_cb(TimerHandle_t timer);

const esp_partition_t *app_fring_partition;
static esp_event_loop_handle_t app_event_loop;

static uint8_t privilege_to_slots = 0x00;
TimerHandle_t servo_close_timer;
static board_servo_t servo;

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
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) 
	{
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
	access_init(); /* all nvs inits */ 
	
	// create timer which wait 3 secs and close servos
	servo_close_timer = xTimerCreate("servo", pdMS_TO_TICKS(3000), pdFALSE, NULL, servo_close_cb);
	ESP_ERROR_CHECK(servo_close_timer == NULL ? ESP_ERR_NO_MEM : ESP_OK);
}

static void servo_close_cb(TimerHandle_t timer)
{
	(void) timer;
	board_servo_set_angle(servo, CONFIG_UI_SERVO_CLOSE_ANGLE);
	privilege_to_slots = 0x00;
	ESP_LOGD(app_tag, "Slot close");
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
	static uint64_t received_card_id = 0;

	if(event_base == BOARD_EVENT)
	{
		switch(event_id)
		{
			case BOARD_EVENT_NEW_CARD: /* process code */
			{
				// recived valid CTU card ID
				uint64_t *event_card_id = event_data;
				received_card_id = *event_card_id;
				ESP_LOGD(app_tag, "Received card ID: %llu", received_card_id);

				if (access_find_card_id_in_nvs(received_card_id, &privilege_to_slots))
				{	
					ui_rg_beep_open(UI_ACCESS_GRANTED);
				}
				else
				{
					ui_rg_beep_open(UI_ACCESS_DENIED);
					access_save_card_id_in_nvs(received_card_id, 0x05);
				}
				access_set_acl_in_nvs();
				access_get_acl_from_nvs();
				break;
			}
			case BOARD_EVENT_BUTTON:
        	{
				/* start waiting for slot choice */
				xTimerStart(servo_close_timer, 0);
				uint8_t *button = event_data;
            	ESP_LOGD(app_tag, "Button pressed: %d", *button);
				ESP_LOGD(app_tag, "Privilege slots: %d", privilege_to_slots);
				uint8_t button_bit_mask = 0b00000001 << ((*button) - 1);
				if (button_bit_mask & privilege_to_slots)
				{
            		report_data.when = 0;
					/* check access to slots */
					switch(*button)
					{
            			case 1:
							servo = 0;
                			report_data.card_id = received_card_id;
							report_data.slot_id = 1;
                			break;
            			case 2:
							servo = 1;
                			report_data.card_id = received_card_id;
							report_data.slot_id = 2;
                			break;
            			case 3:
							servo = 2;
                			report_data.card_id = received_card_id;
							report_data.slot_id = 3;
                			break;
            			default:
                			break;
            		}
					board_servo_set_angle(servo, CONFIG_UI_SERVO_OPEN_ANGLE);
					report_data.kind = REPORT_KIND_SLOT_OPEN;
					report_add(&report_data);
					report_data.kind = REPORT_KIND_NEW_CARD;
					report_add(&report_data);
				}
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
