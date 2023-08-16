#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "driver/ledc.h"
#include "driver/rmt.h"
#include "driver/mcpwm.h"
#include "esp_adc_cal.h"
#include "esp_log.h"
#include "esp_event.h"
#include "board_lib.h"

#define LED_TIM LEDC_TIMER_0
#define LED_MODE LEDC_HIGH_SPEED_MODE

typedef struct {
	mcpwm_unit_t unit;
	mcpwm_timer_t timer;
 	mcpwm_io_signals_t io_signal;
	mcpwm_generator_t gen;
	int gpio;
} servo_t;

servo_t servo_conf[BOARD_SERVO_MAX] = {
	{MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM0A, MCPWM_OPR_A, CONFIG_BOARD_SERVO_1_GPIO},
	{MCPWM_UNIT_0, MCPWM_TIMER_1, MCPWM1A, MCPWM_OPR_A, CONFIG_BOARD_SERVO_2_GPIO},
	{MCPWM_UNIT_0, MCPWM_TIMER_2, MCPWM2A, MCPWM_OPR_A, CONFIG_BOARD_SERVO_3_GPIO},
};

ESP_EVENT_DEFINE_BASE(BOARD_EVENT);

static void button_gpio_isr(void* arg);
static void button_timer_cb(TimerHandle_t timer);

static esp_event_loop_handle_t board_event_loop;
TimerHandle_t board_button_timer;

static inline uint32_t convert_servo_angle_to_duty_us(int angle)
{
    return (angle + CONFIG_BOARD_SERVO_MAX_DEGREE) * (CONFIG_BOARD_SERVO_MAX_PULSEWIDTH_US - CONFIG_BOARD_SERVO_MIN_PULSEWIDTH_US) / (2 * CONFIG_BOARD_SERVO_MAX_DEGREE) + CONFIG_BOARD_SERVO_MIN_PULSEWIDTH_US;
}

/* call once before using other board functions, provide event loop for button */
void board_init(esp_event_loop_handle_t event_loop)
{
	ledc_timer_config_t timer_conf;
	ledc_channel_config_t ledc_conf;
	mcpwm_config_t pwm_conf;

	board_event_loop = event_loop;

	/* output GPIOs */
	gpio_config_t gpio_out_conf = {
			.pin_bit_mask = 1ULL<<CONFIG_BOARD_BUZZ_GPIO | 1ULL<<CONFIG_BOARD_RELAY_GPIO | 1ULL<<CONFIG_BOARD_READER_EN_GPIO | 1ULL<<CONFIG_BOARD_READER_TRG_GPIO,
			.mode = GPIO_MODE_OUTPUT,
			.pull_up_en = GPIO_PULLUP_DISABLE,
			.pull_down_en = GPIO_PULLDOWN_DISABLE,
			.intr_type = GPIO_INTR_DISABLE,
	};
	ESP_ERROR_CHECK(gpio_config(&gpio_out_conf));

	/* UI LEDS */
	timer_conf.clk_cfg = LEDC_AUTO_CLK;
	timer_conf.duty_resolution = LEDC_TIMER_10_BIT;
	timer_conf.freq_hz = 2000;
	timer_conf.speed_mode = LED_MODE;
	timer_conf.timer_num = LED_TIM;
	ESP_ERROR_CHECK(ledc_timer_config(&timer_conf));
	ledc_conf.flags.output_invert = false;
	ledc_conf.hpoint = 0;
	ledc_conf.intr_type = LEDC_INTR_DISABLE;
	ledc_conf.speed_mode = LED_MODE;
	ledc_conf.timer_sel = LED_TIM;
	ledc_conf.channel = BOARD_RED_CH;
	ledc_conf.duty = 0;
	ledc_conf.gpio_num = CONFIG_BOARD_LED_R_GPIO;
	ESP_ERROR_CHECK(ledc_channel_config(&ledc_conf));
	ledc_conf.channel = BOARD_GREEN_CH;
	ledc_conf.gpio_num = CONFIG_BOARD_LED_G_GPIO;
	ESP_ERROR_CHECK(ledc_channel_config(&ledc_conf));
	ledc_conf.channel = BOARD_BLUE_CH;
	ledc_conf.gpio_num = CONFIG_BOARD_LED_B_GPIO;
	ESP_ERROR_CHECK(ledc_channel_config(&ledc_conf));

	/* button debounce timer */
	board_button_timer = xTimerCreate("btn", pdMS_TO_TICKS(CONFIG_BOARD_BUTTON_DEBOUNCE), pdFALSE, NULL, button_timer_cb);
	ESP_ERROR_CHECK(board_button_timer == NULL ? ESP_ERR_NO_MEM : ESP_OK);
	/* button pin ISR */
	ESP_ERROR_CHECK(gpio_set_intr_type(CONFIG_BOARD_BUTTON_GPIO, GPIO_INTR_NEGEDGE));
	ESP_ERROR_CHECK(gpio_install_isr_service(0));
	ESP_ERROR_CHECK(gpio_isr_handler_add(CONFIG_BOARD_BUTTON_GPIO, button_gpio_isr, NULL));

<<<<<<< HEAD
	pwm_conf.frequency = 50; // frequency = 50Hz, i.e. for every servo motor time period should be 20ms
	pwm_conf.cmpr_a = 0;     // duty cycle of PWMxA = 0
 	pwm_conf.counter_mode = MCPWM_UP_COUNTER;
	pwm_conf.duty_mode = MCPWM_DUTY_MODE_0;
=======
    gpio_out_conf.pin_bit_mask = 1ULL<<16 | 1ULL<<17 | 1ULL<<19;
			gpio_out_conf.mode = GPIO_MODE_INPUT;
			gpio_out_conf.pull_up_en = GPIO_PULLUP_ENABLE;
			gpio_out_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
			gpio_out_conf.intr_type = GPIO_INTR_DISABLE;
	ESP_ERROR_CHECK(gpio_config(&gpio_out_conf));
>>>>>>> eeb48b7 (First implementation of button drivers)
	/* servo */
	unsigned int i;
	for (i =0 ; i < BOARD_SERVO_MAX; i++) {
		mcpwm_gpio_init(servo_conf[i].unit, servo_conf[i].io_signal, servo_conf[i].gpio); // To drive a RC servo, one MCPWM generator is enough
		mcpwm_init(servo_conf[i].unit, servo_conf[i].timer, &pwm_conf);
		ESP_ERROR_CHECK(mcpwm_set_duty_in_us(servo_conf[i].unit, servo_conf[i].timer, servo_conf[i].gen, convert_servo_angle_to_duty_us(CONFIG_BOARD_SERVO_INIT_ANGLE)));
	}
}

/* buzzer on/off control */
void board_set_buzzer(bool state)
{
	ESP_ERROR_CHECK(gpio_set_level(CONFIG_BOARD_BUZZ_GPIO, state));
}

/* LED brightness control */
void board_set_led(ledc_channel_t led_ch, uint32_t duty)
{
	ESP_ERROR_CHECK(ledc_set_duty(LED_MODE, led_ch, duty));
	ESP_ERROR_CHECK(ledc_update_duty(LED_MODE, led_ch));
}

/* relay on/off control */
void board_set_relay(bool state)
{
	ESP_ERROR_CHECK(gpio_set_level(CONFIG_BOARD_RELAY_GPIO, state));
}

/* called at falling edge */
static void button_gpio_isr(void* arg)
{
	BaseType_t need_yield = pdFALSE;

	xTimerStartFromISR(board_button_timer, &need_yield);
	if(need_yield)
		portYIELD_FROM_ISR();
}

static void button_timer_cb(TimerHandle_t timer)
{
	(void)timer;

    static uint8_t button = 0;
	/* send event if still pressed */
	if(!gpio_get_level(CONFIG_BOARD_BUTTON_GPIO))
	{
        if(!gpio_get_level(16)){
            button = 1;
        } else
        if(!gpio_get_level(17)){
            button = 2;
        } else
        if(!gpio_get_level(19)){
            button = 3;
        }
        else {
            button = 0;
        }
		ESP_ERROR_CHECK(esp_event_post_to(board_event_loop, BOARD_EVENT, BOARD_EVENT_BUTTON, &button, sizeof(button), 0));
	}
}

void board_servo_set_angle(board_servo_t servo, int angle)
{
	ESP_ERROR_CHECK(mcpwm_set_duty_in_us(servo_conf[servo].unit, servo_conf[servo].timer, servo_conf[servo].gen, convert_servo_angle_to_duty_us(angle)));
}
