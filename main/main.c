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
#include "cloud_manager.h"
// #include "ui_manager.h"
#include "led_manager.h"
#include "report_manager.h"
#include "access_manager.h"

#define SERVO_OPEN_PERIOD pdMS_TO_TICKS(3000)

static const char *app_tag = "app";

static void app_event_cb(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void servo_close_cb(TimerHandle_t timer);
static void remove_privilages_cb(TimerHandle_t timer);

const esp_partition_t *app_fring_partition;
static esp_event_loop_handle_t app_event_loop;

TimerHandle_t remove_privilages_timer;
static uint8_t privilege_to_slots = 0x00;

TimerHandle_t servo_close_timer;
static board_servo_t servo;

typedef enum 
{
	SLOT_1,
	SLOT_2,
	SLOT_3,
	SLOT_MAX
};

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
	// ui_start();
	led_start();
	wifi_init(); /* connects to network if configured in the NVS */
	report_start(app_fring_partition); /* saves and uploads reports */
	board_reader_start(app_event_loop, TP_READER); /* reads QR codes */
	cloud_init(app_event_loop); /* connects to cloud if configured in the NVS */
	access_init(); /* all nvs inits */ 
	
	/* create timer which wait 3 secs and close servos */
	servo_close_timer = xTimerCreate("servo", SERVO_OPEN_PERIOD, pdFALSE, NULL, servo_close_cb);
	ESP_ERROR_CHECK(servo_close_timer == NULL ? ESP_ERR_NO_MEM : ESP_OK);
	
	/* idle state waiting for card scanning  */
	led_task_notify(LED_NOTIFY_IDLE);
	
	/* create timer to revoke privilages to slots */	
	remove_privilages_timer = xTimerCreate("remove_privilages", LED_SLOT_SEL_PERIOD, pdFALSE, NULL, remove_privilages_cb);
	ESP_ERROR_CHECK(remove_privilages_cb == NULL ? ESP_ERR_NO_MEM : ESP_OK);
}

static void servo_close_cb(TimerHandle_t timer)
{
	(void) timer;
	board_servo_set_angle(servo, CONFIG_UI_SERVO_CLOSE_ANGLE);
	privilege_to_slots = 0;
	ESP_LOGD(app_tag, "Slot close");
}

static void remove_privilages_cb(TimerHandle_t timer)
{
	(void) timer;
	privilege_to_slots = 0;
	ESP_LOGD(app_tag, "Removed privilages");
	led_task_notify(LED_NOTIFY_IDLE);
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
				/* recived valid CTU card ID */
				uint64_t *event_card_id = event_data;
				received_card_id = *event_card_id;
				ESP_LOGD(app_tag, "Received card ID: %llu", received_card_id);

				if (access_find_card_id_in_nvs(received_card_id, &privilege_to_slots))
				{	
					uint32_t slot_bit_mask;
					uint8_t slot;
					for (slot = 0; slot < SLOT_MAX; slot++)	
					{
						slot_bit_mask = (uint32_t) 1 << slot;
						if (slot_bit_mask & privilege_to_slots)
							led_task_notify(slot_bit_mask);
					}

					xTimerStart(remove_privilages_timer, 0);
					break;
				}
				else
				{
					led_task_notify(LED_NOTIFY_ACCESS_DENIED);
					report_data.kind = REPORT_KIND_NEW_CARD;
					report_data.card_id = received_card_id;
					report_add(&report_data);
				}
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
				uint8_t button_bit_mask = (uint8_t) 1 << ((*button) - 1);
				if (button_bit_mask & privilege_to_slots)
				{
            		report_data.when = 0;
					/* check access to slots */
					switch(*button)
					{
            			case 1:
							servo = 0;
                			break;
            			case 2:
							servo = 1;
                			break;
            			case 3:
							servo = 2;
                			break;
            			default:
                			break;
            		}
					report_data.card_id = received_card_id;
                	report_data.slot_id = servo + 1;
					board_servo_set_angle(servo, CONFIG_UI_SERVO_OPEN_ANGLE);
					report_data.kind = REPORT_KIND_SLOT_OPEN;
					report_add(&report_data);
				}
            	break;
            }
		}
		return;
	}
	if(event_base == IP_EVENT) /* only IP_EVENT_STA_GOT_IP */
	{
		led_task_notify(LED_NOTIFY_IDLE);
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
			led_task_notify(LED_NOTIFY_NO_WIFI);
		}
		return;
	}
	if(event_base == CLOUD_EVENT)
	{
		switch(event_id)
		{
			case CLOUD_EVENT_CONNECTED:
				led_task_notify(LED_NOTIFY_IDLE);
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
				led_task_notify(LED_NOTIFY_NO_CLOUD);
				break;
			case CLOUD_EVENT_LED: /* RPC led, parameter: number 0 - 256 */
				led_brg = *(int *)event_data;
				if(led_brg > LED_MAX_LED_BRG)
					led_brg = LED_MAX_LED_BRG;
				led_brightness_set(led_brg);
				break;
			case CLOUD_EVENT_OPEN: /* RPC parameter: number 1 - grant access, other - deny */
				if(*(int *)event_data == 1)
				{
					//ui_rg_beep_open(UI_ACCESS_GRANTED);
				}
				else
				{
					//ui_rg_beep_open(UI_ACCESS_DENIED);
				}
				break;
		}
		return;
	}
}
