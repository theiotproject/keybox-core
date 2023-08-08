#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_rom_crc.h"
#include "esp_rom_gpio.h"
#include "esp_event.h"
#include "board_lib.h"

#define READER_UART UART_NUM_1

static void ctu_task(void *arg);

static const char *ctu_tag = "ctu";

static TaskHandle_t ctu_task_handle;
static esp_event_loop_handle_t ctu_event_loop;

/* inits reader and starts reader task, posts codes to the provided event loop */
void board_reader_start(esp_event_loop_handle_t event_loop, UBaseType_t task_priority)
{
	BaseType_t ret;

	ctu_event_loop = event_loop;
	ret = xTaskCreate(ctu_task, ctu_tag, 2048 + configMINIMAL_STACK_SIZE, NULL, task_priority, &ctu_task_handle);
	ESP_ERROR_CHECK(ret != pdPASS ? ESP_ERR_NO_MEM : ESP_OK);
}

static void ctu_task(void *arg)
{
	uart_config_t uart_conf;
	size_t i;
	ssize_t code_pos;
	uint8_t *code_buf = NULL;
	uart_event_t uart_event;
	QueueHandle_t uart_queue;
	uint8_t read_data;

	(void)arg;

	/* interface */
	uart_conf.baud_rate = 115200;
	uart_conf.data_bits = UART_DATA_8_BITS;
	uart_conf.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
	uart_conf.parity = UART_PARITY_DISABLE;
	uart_conf.source_clk = UART_SCLK_APB;
	uart_conf.stop_bits = UART_STOP_BITS_1;
	uart_conf.rx_flow_ctrl_thresh = 64;
	ESP_ERROR_CHECK(uart_param_config(READER_UART, &uart_conf));
	ESP_ERROR_CHECK(uart_driver_install(READER_UART, 2*(CONFIG_MAX_CODE_LEN + 2), 0, 2, &uart_queue, 0)); /* 0 buffer for TX - block until completion */
	ESP_ERROR_CHECK(uart_set_pin(READER_UART, CONFIG_BOARD_READER_TXD_GPIO, CONFIG_BOARD_READER_RXD_GPIO, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

	ESP_ERROR_CHECK(uart_flush(READER_UART));
	
	/* alocate space for code */
	code_buf = malloc(CONFIG_MAX_CODE_LEN+1);
	ESP_ERROR_CHECK(code_buf == NULL ? ESP_ERR_NO_MEM : ESP_OK);

	/* main reader loop */
	code_pos = -1;
	while(true)
	{
		if(xQueueReceive(uart_queue, (void *)&uart_event, portMAX_DELAY))
		{
			switch(uart_event.type)
			{
			case UART_DATA:
				for(i=0; i<uart_event.size; i++)
				{
					uart_read_bytes(READER_UART, &read_data, 1, portMAX_DELAY);
					if(code_pos < 0) /* before code */
					{
						if(read_data == '\n') /* start marker */
							code_pos = 0;
					}
					else /* inside code */
					{
						if(read_data == '\r') /* end marker */
						{
							/* report new code */
							code_buf[code_pos] = 0; /* terminate string */
							ESP_LOGD(ctu_tag, "New code: %s", code_buf);
							ESP_ERROR_CHECK(esp_event_post_to(ctu_event_loop, BOARD_EVENT, BOARD_EVENT_NEW_CARD, code_buf, code_pos+1, portMAX_DELAY));
							code_pos = -1; /* search again */
						}
						else if(code_pos >= CONFIG_MAX_CODE_LEN) /* oversized */
						{
							ESP_LOGW(ctu_tag, "Oversized code");
							code_pos = -1; /* search again */
						}
						else /* continue */
						{
							code_buf[code_pos] = read_data;
							code_pos++;
						}
					}
				}
				break;
			case UART_FIFO_OVF:
			case UART_BUFFER_FULL:
				ESP_LOGW(ctu_tag, "UART overflow");
				ESP_ERROR_CHECK(uart_flush_input(READER_UART));
				xQueueReset(uart_queue);
				break;
			default:
				ESP_LOGD(ctu_tag, "UART event type: %d", uart_event.type);
			}
		}
	}
}