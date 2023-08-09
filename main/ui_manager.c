#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs.h"
#include "task_prio.h"
#include "board_lib.h"
#include "ui_manager.h"

/* open for this TQ count */
#define OPEN_COUNTS (CONFIG_UI_OPEN_TIME/CONFIG_UI_TQ)
/* maximum heat points accumulated */
#define HEAT_MAX ((1000*CONFIG_UI_OPEN_PERIOD/CONFIG_UI_TQ)*CONFIG_UI_OPEN_DUTY*(100-CONFIG_UI_OPEN_DUTY)/100)

typedef enum
{
	UI_ITEM_LED_R,
	UI_ITEM_LED_G,
	UI_ITEM_LED_B,
	UI_ITEM_BEEP,
	UI_ITEM_OPEN /* without pattern */
} ui_item_t;

typedef struct
{
	uint32_t item_map;
	uint32_t pattern[UI_ITEM_OPEN];
} ui_update_msg_t;

//static void ui_timer_cb(TimerHandle_t xTimer);
static void ui_task(void *arg);

static const char *ui_tag = "ui";
/* maximum LED duty, RGB */
static const uint32_t ui_led_max[3] = {BOARD_LED_MAX, BOARD_LED_MAX/4, BOARD_LED_MAX/2};

/* governs pattern update */
static QueueHandle_t ui_update_queue;
/* global brightness 0 - 256 */
static uint32_t ui_brightnesss;
static nvs_handle_t ui_nvs_handle;

void ui_start(void)
{
	BaseType_t ret;

	/* read LED brightness from storage */
	ESP_ERROR_CHECK(nvs_open(ui_tag, NVS_READWRITE, &ui_nvs_handle));
	ui_brightnesss = CONFIG_UI_DEF_BRG;
	if(nvs_get_u32(ui_nvs_handle, "led_brg", &ui_brightnesss) == ESP_OK)
		if(ui_brightnesss > 256)
			ui_brightnesss = CONFIG_UI_DEF_BRG;
	/* queue for updates */
	ui_update_queue = xQueueCreate(8, sizeof(ui_update_msg_t));
	ESP_ERROR_CHECK(ui_update_queue == NULL ? ESP_ERR_NO_MEM : ESP_OK);
	/* timing for blink and beep patterns */
	ret = xTaskCreate(ui_task, ui_tag, 2048 + configMINIMAL_STACK_SIZE, NULL, TP_UI, NULL);
	ESP_ERROR_CHECK(ret != pdPASS ? ESP_ERR_NO_MEM : ESP_OK);
}

/* global brightness 0 to 256 */
void ui_brightness(uint32_t brightness)
{
	ui_brightnesss = brightness;
	nvs_set_u32(ui_nvs_handle, "led_brg", ui_brightnesss);
	nvs_commit(ui_nvs_handle);
}

/* set beep and red, green LEDs */
void ui_rg_beep_open(uint32_t r, uint32_t g, uint32_t beep, bool open)
{
	ui_update_msg_t msg;

	msg.item_map = BIT(UI_ITEM_BEEP) | BIT(UI_ITEM_LED_R) | BIT(UI_ITEM_LED_G);
	msg.pattern[UI_ITEM_LED_R] = r;
	msg.pattern[UI_ITEM_LED_G] = g;
	msg.pattern[UI_ITEM_BEEP] = beep;
	if(open){
		msg.item_map |= BIT(UI_ITEM_OPEN);
	}
	xQueueSend(ui_update_queue, &msg, 0);
}

/* set blue LED  */
void ui_blue(uint32_t b)
{
	ui_update_msg_t msg;

	msg.item_map = BIT(UI_ITEM_LED_B);
	msg.pattern[UI_ITEM_LED_B] = b;
	xQueueSend(ui_update_queue, &msg, 0);
}

static void ui_task(void *arg)
{
	static uint32_t ui_open_heat = 0;
	static uint32_t ui_pattern[UI_ITEM_OPEN] = {0, 0, UI_NET_NO_WIFI, 0x7}; /* no connection, start with a beep */
	static uint32_t ui_open_counter = 0;
	ui_update_msg_t update_msg;
	ui_item_t i;
	bool lsb;
	(void)arg;

	while(true)
	{
		vTaskDelay(pdMS_TO_TICKS(CONFIG_UI_TQ));
		/* check for commands */
		while(xQueueReceive(ui_update_queue, (void *)&update_msg, 0))
		{
			/* update patterns */
			for(i=0; i<UI_ITEM_OPEN; i++)
				if(update_msg.item_map & BIT(i))
					ui_pattern[i] = update_msg.pattern[i];
			/* control output enable, limit duty cycle within a defined period */
			if(update_msg.item_map & BIT(UI_ITEM_OPEN))
			{
				if(ui_open_heat <= HEAT_MAX) /* include equal, to allow operation with a 100% duty cycle */
				{
					ESP_LOGD(ui_tag, "Open start, heat %u", ui_open_heat);
					board_set_relay(true);
					ui_open_counter = OPEN_COUNTS + 1; /* decreased immediately after */
				}
				else
				{
					ESP_LOGD(ui_tag, "Open cancel, heat %u", ui_open_heat);
				}
			}
		}
		/* execute patterns */
		for(i=0; i<UI_ITEM_OPEN; i++)
		{
			lsb = ui_pattern[i]&1;
			switch(i)
			{
			case UI_ITEM_LED_R:
			case UI_ITEM_LED_G:
			case UI_ITEM_LED_B:
				board_set_led(i, (uint32_t)lsb*((ui_brightnesss*ui_led_max[i])>>8));
				break;
			case UI_ITEM_BEEP:
				board_set_buzzer(lsb);
				break;
			default:
				break;
			}
			if(ui_pattern[i] & UI_PATTERN_LOOP)
			{
				ui_pattern[i] &= ~UI_PATTERN_LOOP;
				ui_pattern[i] >>= 1;
				ui_pattern[i] |= UI_PATTERN_LOOP | (uint32_t)lsb<<30;
			}
			else
			{
				ui_pattern[i] >>= 1;
			}
		}
		/* control output disable after a defined time */
		if(ui_open_counter)
		{
			ui_open_counter--;
			if(!ui_open_counter)
			{
				ESP_LOGD(ui_tag, "Open finish, heat %u", ui_open_heat);
				board_set_relay(false);
			}
		}
		/* duty cycle tracking */
		if(ui_open_counter)
		{
			ui_open_heat += (100-CONFIG_UI_OPEN_DUTY);
		}
		else if(ui_open_heat > CONFIG_UI_OPEN_DUTY)
		{
			ui_open_heat -= CONFIG_UI_OPEN_DUTY;
		}
		else
		{
			ui_open_heat = 0;
		}
	}
}
