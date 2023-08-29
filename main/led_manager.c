#ifndef MAIN_LED_MANAGER_H_
#define MAIN_LED_MANAGER_H_

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs.h"
#include "task_prio.h"
#include "board_lib.h"
#include "led_manager.h"

/*
notify patterns

CLOUD & CONF

* NO_CONF
* NO_WIFI
* NO_CLOUD

ACCESS CTRL

* IDLE
* ACCESS_DENIED
* ACCESS_GRANTED_SLOT_SEL
* ACCESS_GRANTED_KEY_DIST
* ACCESS_GRANTED_NO_PRIV

interfejs który włacza i wyłącza timer
główny task loguje co 0.5 sec
logowanie jest blokowanie prez sem bin

ui_task <- timer
*/

static void led_task(void *arg);

static const char *led_tag = "led";
static TaskHandle_t led_task_handle;

static uint32_t led_brightness;
static nvs_handle_t led_nvs_handle;


void led_start(void)
{
	BaseType_t ret;

	/* read LED brightness from storage */
	ESP_ERROR_CHECK(nvs_open(led_tag, NVS_READWRITE, &led_nvs_handle));
	led_brightness = CONFIG_UI_DEF_BRG;
	if(nvs_get_u32(led_nvs_handle, "led_brg", &led_brightness) == ESP_OK)
		if(led_brightness > 256)
			led_brightness = CONFIG_UI_DEF_BRG;
	/* queue for updates */
	// ui_update_queue = xQueueCreate(8, sizeof(ui_update_msg_t));
	// ESP_ERROR_CHECK(ui_update_queue == NULL ? ESP_ERR_NO_MEM : ESP_OK);
	/* timing for blink and beep patterns */
	ret = xTaskCreate(led_task, led_tag, 2048 + configMINIMAL_STACK_SIZE, NULL, TP_LED, &led_task_handle);
	ESP_ERROR_CHECK(ret != pdPASS ? ESP_ERR_NO_MEM : ESP_OK);
}

/* global brightness 0 to 256 */
void led_brightness_set(uint32_t brightness)
{
	led_brightness = brightness;
	nvs_set_u32(led_nvs_handle, "led_brg", led_brightness);
	nvs_commit(led_nvs_handle);
}

static void led_task(void *arg)
{
	(void)arg;
	uint32_t ulNotifiedValue;

	/* main task loop */
	while (true)
	{
		xTaskNotifyWait(0, ULONG_MAX, &ulNotifiedValue, portMAX_DELAY);
		if (ulNotifiedValue & LED_NOTIFY_IDLE)
		{
			vTaskDelay(1000 / portTICK_PERIOD_MS);
			ESP_LOGD(led_tag, "LED_NOTIFY_IDLE");
			led_pattern_idle();
		}
	}
}

void led_task_notify(uint32_t ulValuePattern)
{
	xTaskNotify(led_task_handle, ulValuePattern, eSetBits);
}

void led_pattern_idle(void)
{
	board_set_led(LED_ITEM_B, BOARD_LED_MAX);
}

#endif /* MAIN_LED_MANAGER_H_ */