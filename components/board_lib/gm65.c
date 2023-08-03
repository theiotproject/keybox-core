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
#define GM65_TYPE_RESP 0x00
#define GM65_TYPE_RD 0x07
#define GM65_TYPE_WR 0x08
#define GM65_TYPE_SAVE 0x09
#define GM65_DATA_POS 4

#define INFO_FORMAT(tab) "HWrev 0x%02hhX SWrev 0x%02hhX SWd %hhu.%02hhu.%d", (tab)[0], (tab)[1], (tab)[4], (tab)[3], 2000 + (int)(tab)[2]

static void gm65_task(void *arg);
static void gm65_power(uint32_t state);
static esp_err_t gm65_speed(void);
static esp_err_t gm65_read_reg(uint16_t addr, uint8_t *data, size_t size);
static esp_err_t gm65_write_reg(uint16_t addr, const uint8_t *data, size_t size);
static void gm65_transimt(uint16_t addr, uint8_t type, const uint8_t *data, size_t size);
static uint8_t *gm65_receive(uint8_t expected_type, size_t expected_size);

static const char *gm65_tag = "gm65";
/* registers configuration, address LSB, value */
static const uint8_t gm65_config[][2] = {
		{0x03, 0x02}, /* disable configuration by codes */
		{0x07, 0x00}, /* disable auto-sleep */
		{0x2c, 0x00}, /* disable all codes */
		{0x3f, 0x01}, /* enable QR */
		{0x62, 0x10}, /* set prefix length to 1 character */
		{0x63, '\n'}, /* set prefix character to LF */
		{0x60, 0x09}, /* enable prefix and CR end character */
		{0x00, 0x03}, /* switch to sensing mode, no light, no aim */
		{0x0f, 0x32}, /* set sensitivity */
		{0x10, 0x0A}, /* set sensitivity */
		{0x04, 2}, /* stabilization time: 0.2s */
		{0x04, 5}, /* idle time after scan: 0.5s */
		{0x06, 30},/* scan time after detection: 3s */
		{0x13, 0x80+40}, /* same code blanking time: 4s */
		{0x0d, 0x00}, /* send code data on UART */
};

static TaskHandle_t gm65_task_handle;
static esp_event_loop_handle_t gm65_event_loop;

/* inits reader and starts reader task, posts codes to the provided event loop */
void board_reader_start(esp_event_loop_handle_t event_loop, UBaseType_t task_priority)
{
	BaseType_t ret;

	gm65_event_loop = event_loop;
	ret = xTaskCreate(gm65_task, gm65_tag, 2048 + configMINIMAL_STACK_SIZE, NULL, task_priority, &gm65_task_handle);
	ESP_ERROR_CHECK(ret != pdPASS ? ESP_ERR_NO_MEM : ESP_OK);
}

static void gm65_task(void *arg)
{
	uart_config_t uart_conf;
	size_t i;
	ssize_t code_pos;
	uint8_t *code_buf = NULL;
	uart_event_t uart_event;
	QueueHandle_t uart_queue;
	size_t reader_info_size;
	char *reader_info_str;
	uint8_t version[5];
	uint8_t read_data;

	(void)arg;

	/* power control GPIO */
	const gpio_config_t gpio_out_conf = {
			.pin_bit_mask = 1ULL<<CONFIG_BOARD_READER_EN_GPIO,
			.mode = GPIO_MODE_OUTPUT,
			.pull_up_en = GPIO_PULLUP_DISABLE,
			.pull_down_en = GPIO_PULLDOWN_DISABLE,
			.intr_type = GPIO_INTR_DISABLE,
	};
	ESP_ERROR_CHECK(gpio_config(&gpio_out_conf));
	gm65_power(0); /* ensure power cycle */
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
	/* enable power */
	gm65_power(1);
	ESP_ERROR_CHECK(uart_flush(READER_UART));
	/* detect/set speed */
	if(gm65_speed() != ESP_OK)
		goto board_reader_task_fail;
	/* show revisions */
	if(gm65_read_reg(0x00e1, version, 5) != ESP_OK)
		goto board_reader_task_fail;
	ESP_LOGD(gm65_tag, INFO_FORMAT(version));
	/* configure reader */
	for(i=0; i<sizeof(gm65_config)/sizeof(gm65_config[0]); i++)
	{
		if(gm65_read_reg((uint16_t)gm65_config[i][0], &read_data, 1) != ESP_OK)
			goto board_reader_task_fail;
		if(read_data == gm65_config[i][1]) /* skip already set */
			continue;
		if(gm65_write_reg((uint16_t)gm65_config[i][0], &gm65_config[i][1], 1) != ESP_OK)
			goto board_reader_task_fail;
	}
	/* alocate space for code */
	code_buf = malloc(CONFIG_MAX_CODE_LEN+1);
	ESP_ERROR_CHECK(code_buf == NULL ? ESP_ERR_NO_MEM : ESP_OK);
	/* enable IR illumination */
	board_set_ir(1);
	reader_info_size = snprintf(NULL, 0, "Rt GM65 " INFO_FORMAT(version)) + 1;
	reader_info_str = malloc(reader_info_size);
	sprintf(reader_info_str, "Rt GM65 " INFO_FORMAT(version));
	/* inform application */
	ESP_ERROR_CHECK(esp_event_post_to(gm65_event_loop, BOARD_EVENT, BOARD_EVENT_READER_READY, reader_info_str, reader_info_size, portMAX_DELAY));
	free(reader_info_str);
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
							ESP_LOGD(gm65_tag, "New code: %s", code_buf);
							ESP_ERROR_CHECK(esp_event_post_to(gm65_event_loop, BOARD_EVENT, BOARD_EVENT_NEW_CODE, code_buf, code_pos+1, portMAX_DELAY));
							code_pos = -1; /* search again */
						}
						else if(code_pos >= CONFIG_MAX_CODE_LEN) /* oversized */
						{
							ESP_LOGW(gm65_tag, "Oversized code");
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
				ESP_LOGW(gm65_tag, "UART overflow");
				ESP_ERROR_CHECK(uart_flush_input(READER_UART));
				xQueueReset(uart_queue);
				break;
			default:
				ESP_LOGD(gm65_tag, "UART event type: %d", uart_event.type);
			}
		}
	}
board_reader_task_fail:
	if(code_buf)
		free(code_buf);
	board_set_ir(0);
	gm65_power(0);
	uart_driver_delete(READER_UART);
	ESP_LOGE(gm65_tag, "Failed");
	vTaskDelete(gm65_task_handle);
}

/* controls power supply for reader */
static void gm65_power(uint32_t state)
{
	ESP_ERROR_CHECK(gpio_set_level(CONFIG_BOARD_READER_EN_GPIO, state));
	/* prevent parasitic power through UART */
	if(state)
	{
		ESP_ERROR_CHECK(gpio_set_pull_mode(CONFIG_BOARD_READER_RXD_GPIO, GPIO_PULLUP_ONLY));
		ESP_ERROR_CHECK(gpio_set_direction(CONFIG_BOARD_READER_TXD_GPIO, GPIO_MODE_OUTPUT));
		esp_rom_gpio_connect_out_signal(CONFIG_BOARD_READER_TXD_GPIO, U1TXD_OUT_IDX, false, false);
		vTaskDelay(pdMS_TO_TICKS(CONFIG_BOARD_READER_ON_DELAY));
	}
	else
	{
		ESP_ERROR_CHECK(gpio_set_pull_mode(CONFIG_BOARD_READER_RXD_GPIO, GPIO_PULLDOWN_ONLY));
		ESP_ERROR_CHECK(gpio_set_direction(CONFIG_BOARD_READER_TXD_GPIO, GPIO_MODE_DISABLE));
		vTaskDelay(pdMS_TO_TICKS(CONFIG_BOARD_READER_OFF_DELAY));
	}
}

/* detects and sets speed if necessary */
static esp_err_t gm65_speed(void)
{
	const uint32_t baudrate[] = {115200, 9600}; /* begin with 115200 */
	size_t i;
	uint8_t reg_data[2];

	i = 0;
	while(true)
	{
		if(gm65_read_reg(0x00e0, reg_data, 1) == ESP_OK)
		{
			if(reg_data[0] == 0x02 || reg_data[0] == 0x05) /* ok */
			{
				ESP_LOGD(gm65_tag, "Detected @ %u", baudrate[i]);
				if(i == 0) /* no change needed */
					return(ESP_OK);
				/* change speed to 115200 */
				reg_data[0] = 0x1a;
				reg_data[1] = 0x00;
				if(gm65_write_reg(0x002a, reg_data, 2) != ESP_OK)
					return(ESP_FAIL);
				ESP_ERROR_CHECK(uart_set_baudrate(READER_UART, baudrate[0]));
				vTaskDelay(pdMS_TO_TICKS(400)); /* reader need time to change baudrate */
				ESP_ERROR_CHECK(uart_flush_input(READER_UART));
				return(ESP_OK);
			}
			else
			{
				ESP_LOGD(gm65_tag, "Wrong data 0x%02hhX @ %u", reg_data[0], baudrate[i]);
			}
		}
		/* next speed */
		i++;
		if(i >= sizeof(baudrate)/sizeof(baudrate[0]))
			return(ESP_FAIL);
		ESP_ERROR_CHECK(uart_set_baudrate(READER_UART, baudrate[i]));
		ESP_ERROR_CHECK(uart_flush(READER_UART));
	}
}

/* complete read operation */
static esp_err_t gm65_read_reg(uint16_t addr, uint8_t *data, size_t size)
{
	uint8_t *rx_buf;
	uint8_t read_size;

	read_size = size & 0xff; /* 256 as 0 */
	gm65_transimt(addr, GM65_TYPE_RD, &read_size, 1);
	rx_buf = gm65_receive(GM65_TYPE_RESP, size);
	if(!rx_buf)
	{
		ESP_LOGD(gm65_tag, "Read %uB @ 0x%04hX failed", size, addr);
		return(ESP_FAIL);
	}
	else
	{
		ESP_LOGD(gm65_tag, "Read %uB @ 0x%04hX data 0x%02hhX", size, addr, rx_buf[GM65_DATA_POS]);
		memcpy(data, &rx_buf[GM65_DATA_POS], size);
	}
	free(rx_buf);
	return(ESP_OK);
}

/* complete write operation */
static esp_err_t gm65_write_reg(uint16_t addr, const uint8_t *data, size_t size)
{
	uint8_t *rx_buf;
	esp_err_t ret = ESP_OK;

	gm65_transimt(addr, GM65_TYPE_WR, data, size);
	rx_buf = gm65_receive(GM65_TYPE_RESP, 1);
	if(!rx_buf)
	{
		ESP_LOGD(gm65_tag, "Write %uB @ 0x%04hX failed, data 0x%02hhX", size, addr, data[0]);
		return(ESP_FAIL);
	}
	else if(rx_buf[GM65_DATA_POS] != 0x00)
	{
		ESP_LOGD(gm65_tag, "Write %uB @ 0x%04hX failed, data 0x%02hhX, response 0x%02hhX", size, addr, data[0], rx_buf[GM65_DATA_POS]);
		ret = ESP_FAIL;
	}
	else
	{
		ESP_LOGD(gm65_tag, "Write %uB @ 0x%04hX OK, data 0x%02hhX", size, addr, data[0]);
	}
	free(rx_buf);
	return(ret);
}

/* writes data at GM65 address */
static void gm65_transimt(uint16_t addr, uint8_t type, const uint8_t *data, size_t size)
{
	uint8_t header[6];
	uint16_t crc;
	uint8_t crc_bytes[2];

	header[0] = 0x7e;
	header[1] = 0x00;
	header[2] = type;
	header[3] = size&0xff; /* 256 encoded as 0 */
	header[4] = addr>>8;
	header[5] = addr&0xff;
	crc = esp_rom_crc16_be((uint16_t)~0x0000, &header[2], 4);
	crc = ~esp_rom_crc16_be(crc, data, size);
	uart_write_bytes(READER_UART, header, sizeof(header));
	uart_write_bytes(READER_UART, data, size);
	crc_bytes[0] = crc>>8;
	crc_bytes[1] = crc&0xff;
	uart_write_bytes(READER_UART, &crc_bytes, sizeof(crc_bytes));
}

/* reads response to command */
static uint8_t *gm65_receive(uint8_t expected_type, size_t expected_size)
{
	uint8_t *buf;
	int ret;
	uint16_t crc_rx;
	uint16_t crc_calc;

	buf = malloc(expected_size + 6); /* include header and CRC */
	ret = uart_read_bytes(READER_UART, buf, expected_size + 6, pdMS_TO_TICKS(CONFIG_BOARD_READER_TIMEOUT));
	if(ret != (int)expected_size + 6) /* timeout or data incomplete */
	{
		ESP_LOGD(gm65_tag, "UART read error %d", ret);
		goto gm65_receive_fail;
	}
	if(buf[0] != 0x02 || buf[1] != 0x00 || buf[2] != expected_type || buf[3] != expected_size)
	{
		ESP_LOGD(gm65_tag, "Incorrect header: head=0x%02hhX%02hhX, type=0x%02hhX (expected 0x%02hhX), length=%hhu (expected %hhu)", buf[0], buf[1], buf[2], expected_type, buf[3], expected_size);
		goto gm65_receive_fail;
	}
	crc_calc = ~esp_rom_crc16_be((uint16_t)~0x0000, &buf[2], expected_size + 2);
	crc_rx = ((uint16_t)buf[expected_size+6-2]<<8) + (uint16_t)buf[expected_size+6-1];
	if(crc_calc != crc_rx)
	{
		ESP_LOGD(gm65_tag, "CRC mismatch: rx=0x%04hX, calc=0x%04hX", crc_rx, crc_calc);
		goto gm65_receive_fail;
	}
	return(buf);
gm65_receive_fail:
	free(buf);
	return(NULL);
}
