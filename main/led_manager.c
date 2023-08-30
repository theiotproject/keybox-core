#ifndef MAIN_LED_MANAGER_H_
#define MAIN_LED_MANAGER_H_

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "nvs.h"
#include "task_prio.h"
#include "board_lib.h"
#include "led_manager.h"

/*

CORE STATE INFO

* NO_CONF
* NO_WIFI
* NO_CLOUD
* IDLE

SLOT ACCESS INFO

* ACCESS_DENIED
* ACCESS_GRANTED_SLOT_SEL
* ACCESS_GRANTED_KEY_DIST
* ACCESS_GRANTED_NO_PRIV

*/

static void led_task(void *arg);
static void led_pattern_idle_cb(void);

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
			ESP_LOGD(led_tag, "Set blue ON");
			TimerHandle_t idle_timer = xTimerCreate("idle", LED_IDLE_CHECK_PERIOD, pdFALSE, NULL, (void*) led_pattern_idle_cb);
			ESP_ERROR_CHECK(idle_timer == NULL ? ESP_ERR_NO_MEM : ESP_OK);
			xTimerStart(idle_timer, portMAX_DELAY);
		}
		if (ulNotifiedValue & LED_NOTIFY_ACCESS_SLOT_1)
		{
			ESP_LOGD(led_tag, "Set red led ON");
			board_set_led(LED_ITEM_B, BOARD_LED_MIN);
			board_set_led(LED_ITEM_R, BOARD_LED_MAX);
			TimerHandle_t slot_sel_timer = xTimerCreate("slot_access_1", LED_SLOT_SEL_PERIOD, pdFALSE, NULL, (void*) led_pattern_idle_cb);
			ESP_ERROR_CHECK(slot_sel_timer == NULL ? ESP_ERR_NO_MEM : ESP_OK);
			xTimerStart(slot_sel_timer, portMAX_DELAY);
		}
		if (ulNotifiedValue & LED_NOTIFY_ACCESS_SLOT_2)
		{
			ESP_LOGD(led_tag, "Set yellow led ON");
			board_set_led(LED_ITEM_B, BOARD_LED_MIN);
			board_set_led(LED_ITEM_Y, BOARD_LED_MAX);
			TimerHandle_t slot_sel_timer = xTimerCreate("slot_access_2", LED_SLOT_SEL_PERIOD, pdFALSE, NULL, (void*) led_pattern_idle_cb);
			ESP_ERROR_CHECK(slot_sel_timer == NULL ? ESP_ERR_NO_MEM : ESP_OK);
			xTimerStart(slot_sel_timer, portMAX_DELAY);
		}
		if (ulNotifiedValue & LED_NOTIFY_ACCESS_SLOT_3)
		{
			ESP_LOGD(led_tag, "Set green led ON");
			board_set_led(LED_ITEM_B, BOARD_LED_MIN);
			board_set_led(LED_ITEM_G, BOARD_LED_MAX);
			TimerHandle_t slot_sel_timer = xTimerCreate("slot_access_3", LED_SLOT_SEL_PERIOD, pdFALSE, NULL, (void*) led_pattern_idle_cb);
			ESP_ERROR_CHECK(slot_sel_timer == NULL ? ESP_ERR_NO_MEM : ESP_OK);
			xTimerStart(slot_sel_timer, portMAX_DELAY);
		}
		if (ulNotifiedValue & LED_NOTIFY_ACCESS_DENIED)
		{
			board_set_led(LED_ITEM_B, BOARD_LED_MIN);
			TimerHandle_t access_denied_timer = xTimerCreate("access denied", LED_ACCESS_DENIED_PERIOD, pdFALSE, NULL, (void*) led_pattern_idle_cb);
			ESP_ERROR_CHECK(access_denied_timer == NULL ? ESP_ERR_NO_MEM : ESP_OK);
			xTimerStart(access_denied_timer, portMAX_DELAY);
			ESP_LOGD(led_tag, "Blink led red");
			for (uint8_t blink = 0; blink < 5; blink++)
			{
				board_set_led(LED_ITEM_R, BOARD_LED_MAX);
				vTaskDelay(100/portTICK_PERIOD_MS);
				board_set_led(LED_ITEM_R, BOARD_LED_MIN);
				vTaskDelay(100/portTICK_PERIOD_MS);
			}
		}
		if (ulNotifiedValue & LED_NOTIFY_NO_WIFI)
		{
			ESP_LOGD(led_tag, "Set blue led OFF");
			board_set_led(LED_ITEM_B, BOARD_LED_MIN);
		}
		if (ulNotifiedValue & LED_NOTIFY_NO_CLOUD)
		{
			ESP_LOGD(led_tag, "Set blue led OFF");
			board_set_led(LED_ITEM_B, BOARD_LED_MIN);
		}
		if (ulNotifiedValue & LED_NOTIFY_LEDS_OFF)
		{
			ESP_LOGD(led_tag, "Set leds OFF");
			/* set all leds OFF */
			uint8_t led_ch;
			for (led_ch = 0; led_ch < LED_ITEM_MAX; led_ch++)
				board_set_led(led_ch, BOARD_LED_MIN);
		}
	}
}

void led_task_notify(uint32_t ulValuePattern)
{
	xTaskNotify(led_task_handle, ulValuePattern, eSetBits);
}

static void led_pattern_idle_cb(void)
{
	/* set all leds OFF */
	uint8_t led_ch;
	for (led_ch = 0; led_ch < LED_ITEM_MAX; led_ch++)
		board_set_led(led_ch, BOARD_LED_MIN);
	/* set blue led ON */
	board_set_led(LED_ITEM_B, BOARD_LED_MAX);
}

#endif /* MAIN_LED_MANAGER_H_ */