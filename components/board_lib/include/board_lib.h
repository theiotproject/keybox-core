#ifndef COMPONENTS_BOARD_LIB_INCLUDE_BOARD_LIB_H_
#define COMPONENTS_BOARD_LIB_INCLUDE_BOARD_LIB_H_

#include "esp_event.h"
#include "driver/ledc.h"

#define BOARD_HW_INFO "ACWT-1A"

#define BOARD_RED_CH LEDC_CHANNEL_0
#define BOARD_GREEN_CH LEDC_CHANNEL_1
#define BOARD_BLUE_CH LEDC_CHANNEL_2
#define BOARD_LED_MAX 1023

typedef enum {
	BOARD_EVENT_READER_READY,
	BOARD_EVENT_NEW_CODE,
	BOARD_EVENT_MAX
} board_event_t;

ESP_EVENT_DECLARE_BASE(BOARD_EVENT);

void board_init(void);
void board_set_buzzer(bool state);
void board_set_led(ledc_channel_t led_ch, uint32_t duty);
void board_set_relay(bool state);
void board_set_ir(uint32_t state);
void board_wiegand_send(uint64_t frame, uint8_t len);
uint32_t board_supply_meas(void);
void board_reader_start(esp_event_loop_handle_t event_loop, UBaseType_t task_priority);

#endif /* COMPONENTS_BOARD_LIB_INCLUDE_BOARD_LIB_H_ */
