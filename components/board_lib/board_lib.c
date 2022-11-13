#include "driver/gpio.h"
#include "driver/adc.h"
#include "driver/ledc.h"
#include "driver/i2c.h"
#include "driver/rmt.h"
#include "esp_adc_cal.h"
#include "esp_log.h"
#include "esp_event.h"
#include "board_lib.h"

#define IR_DUTY (((CONFIG_BOARD_IR_DUTY)<<20)/100 - 1)
#define LED_TIM LEDC_TIMER_0
#define LED_MODE LEDC_HIGH_SPEED_MODE
#define IR_TIM LEDC_TIMER_1
#define IR_MODE LEDC_LOW_SPEED_MODE
#define IR_CH LEDC_CHANNEL_3
#define VM_AVG_CNT 200
#define ADC_READ_CNT VM_AVG_CNT
/* result [mV] */
#define VM_GAIN (int)(1./0.03288*(double)(1<<10))
#define DEFAULT_VREF 1100
#define DO_RMT_CH RMT_CHANNEL_0
#define D1_RMT_CH RMT_CHANNEL_1

ESP_EVENT_DEFINE_BASE(BOARD_EVENT);
static esp_adc_cal_characteristics_t adc1_chars;

/* call once before using other board functions */
void board_init(void)
{
	ledc_timer_config_t timer_conf;
	ledc_channel_config_t ledc_conf;
	i2c_config_t i2c_conf;
	adc_digi_init_config_t adc_init_conf;
	adc_digi_pattern_config_t adc_ptn_conf;
	adc_digi_configuration_t adc_digi_conf;
	rmt_config_t rmt_conf;

	/* output GPIOs */
	const gpio_config_t gpio_out_conf = {
			.pin_bit_mask = 1ULL<<CONFIG_BOARD_BUZZ_GPIO | 1ULL<<CONFIG_BOARD_RELAY_GPIO | 1ULL<<CONFIG_BOARD_READER_EN_GPIO | 1ULL<<CONFIG_BOARD_READER_TRG_GPIO,
			.mode = GPIO_MODE_OUTPUT,
			.pull_up_en = GPIO_PULLUP_DISABLE,
			.pull_down_en = GPIO_PULLDOWN_DISABLE,
			.intr_type = GPIO_INTR_DISABLE,
	};
	ESP_ERROR_CHECK(gpio_config(&gpio_out_conf));
	/* RTC IRQ (not used) */
	ESP_ERROR_CHECK(gpio_set_pull_mode(CONFIG_BOARD_RTC_INT_GPIO, GPIO_PULLUP_ONLY));

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

	/* IR LED */
	timer_conf.clk_cfg = LEDC_AUTO_CLK;
	timer_conf.duty_resolution = LEDC_TIMER_20_BIT;
	timer_conf.freq_hz = CONFIG_BOARD_IR_FREQ;
	timer_conf.speed_mode = IR_MODE;
	timer_conf.timer_num = IR_TIM;
	ESP_ERROR_CHECK(ledc_timer_config(&timer_conf));
	ledc_conf.duty = 0;
	ledc_conf.flags.output_invert = false;
	ledc_conf.hpoint = 0;
	ledc_conf.intr_type = LEDC_INTR_DISABLE;
	ledc_conf.speed_mode = IR_MODE;
	ledc_conf.timer_sel = IR_TIM;
	ledc_conf.channel = IR_CH;
	ledc_conf.gpio_num = CONFIG_BOARD_LED_IR_GPIO;
	ESP_ERROR_CHECK(ledc_channel_config(&ledc_conf));

	/* I2C (used by RTC) */
	i2c_conf.mode = I2C_MODE_MASTER;
	i2c_conf.sda_io_num = CONFIG_BOARD_SDA_GPIO;
	i2c_conf.scl_io_num = CONFIG_BOARD_SCL_GPIO;
	i2c_conf.sda_pullup_en = false;
	i2c_conf.scl_pullup_en = false;
	i2c_conf.master.clk_speed = 100000;
	i2c_conf.clk_flags = 0;
	ESP_ERROR_CHECK(i2c_param_config(CONFIG_RTCD_I2C, &i2c_conf));
	ESP_ERROR_CHECK(i2c_driver_install(CONFIG_RTCD_I2C, i2c_conf.mode, 0, 0, 0));

	/* supply monitor */
	adc_init_conf.adc1_chan_mask = BIT(CONFIG_BOARD_VM_ADC_CH);
	adc_init_conf.adc2_chan_mask = 0;
	adc_init_conf.conv_num_each_intr = ADC_READ_CNT*sizeof(adc_digi_output_data_t);
	adc_init_conf.max_store_buf_size = 1024;
	ESP_ERROR_CHECK(adc_digi_initialize(&adc_init_conf));
	adc_ptn_conf.atten = ADC_ATTEN_DB_0;
	adc_ptn_conf.unit = 0;
	adc_ptn_conf.bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;
	adc_ptn_conf.channel = CONFIG_BOARD_VM_ADC_CH;
	adc_digi_conf.adc_pattern = &adc_ptn_conf;
	adc_digi_conf.conv_limit_en = true;
	adc_digi_conf.conv_limit_num = 250;
	adc_digi_conf.conv_mode = ADC_CONV_SINGLE_UNIT_1;
	adc_digi_conf.format = ADC_DIGI_OUTPUT_FORMAT_TYPE1;
	adc_digi_conf.pattern_num = 1;
	adc_digi_conf.sample_freq_hz = 20*1000;
	ESP_ERROR_CHECK(adc_digi_controller_configure(&adc_digi_conf));
	esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_0, ADC_WIDTH_BIT_DEFAULT, DEFAULT_VREF, &adc1_chars);

	/* wiegand */
	rmt_conf.clk_div = 10; /* 10us tick @ 1MHz clock */
	rmt_conf.flags = RMT_CHANNEL_FLAGS_AWARE_DFS; /* sets 1MHz clock */
	rmt_conf.mem_block_num = 1; /* 63b max */
	rmt_conf.rmt_mode = RMT_MODE_TX;
	rmt_conf.tx_config.carrier_en = false;
	rmt_conf.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
	rmt_conf.tx_config.idle_output_en = true;
	rmt_conf.tx_config.loop_en = false;
	for(rmt_conf.channel=DO_RMT_CH; rmt_conf.channel<=D1_RMT_CH; rmt_conf.channel++)
	{
		rmt_conf.gpio_num = rmt_conf.channel==DO_RMT_CH ? CONFIG_BOARD_WIEGAND_D0_GPIO : CONFIG_BOARD_WIEGAND_D1_GPIO;
		ESP_ERROR_CHECK(rmt_config(&rmt_conf));
		ESP_ERROR_CHECK(rmt_driver_install(rmt_conf.channel, 0, 0));
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

/* IR reader light control */
void board_set_ir(uint32_t state)
{
#if IR_DUTY > 0
	if(!state)
	{
		ESP_ERROR_CHECK(ledc_stop(IR_MODE, IR_CH, 0));
	}
	else
	{
		ESP_ERROR_CHECK(ledc_set_duty(IR_MODE, IR_CH, IR_DUTY));
		ESP_ERROR_CHECK(ledc_update_duty(IR_MODE, IR_CH));
	}
#else
	(void)state;
#endif
}

/* sends Wiegand frame, length in bits, data sent MSB first */
void board_wiegand_send(uint64_t frame, uint8_t len)
{
	static portMUX_TYPE tx_spinlock = (portMUX_TYPE)portMUX_INITIALIZER_UNLOCKED;
	rmt_channel_t ch;
	rmt_item32_t item;
	uint8_t bit_num;
	bool bit_level;

	ESP_ERROR_CHECK(len > 63 ? ESP_ERR_INVALID_ARG : ESP_OK);


	for(ch=DO_RMT_CH; ch<=D1_RMT_CH; ch++)
		ESP_ERROR_CHECK(rmt_tx_memory_reset(ch));
	/* encode each line bit on a symbol pair: inactive - double 0, active - 1 and 0 */
	item.duration0 = CONFIG_BOARD_WIEGAND_PULSE;
	item.duration1  = CONFIG_BOARD_WIEGAND_PERIOD - CONFIG_BOARD_WIEGAND_PULSE;
	item.level1 = 0;
	for(bit_num=0; bit_num<len; bit_num++)
	{
		bit_level = !(frame & (1ULL<<(len-bit_num-1)));

		item.level0 = bit_level;
		ESP_ERROR_CHECK(rmt_fill_tx_items(DO_RMT_CH, &item, 1, bit_num));
		item.level0 = !bit_level;
		ESP_ERROR_CHECK(rmt_fill_tx_items(D1_RMT_CH, &item, 1, bit_num));
	}
	/* end markers */
	item.val = 0;
	for(ch=DO_RMT_CH; ch<=D1_RMT_CH; ch++)
	{
		ESP_ERROR_CHECK(rmt_fill_tx_items(ch, &item, 1, len));
	}
	/* send, ensure no scheduling between calls, sync master isn't available on ESP32 */
	taskENTER_CRITICAL(&tx_spinlock);
	for(ch=DO_RMT_CH; ch<=D1_RMT_CH; ch++)
		rmt_tx_start(ch, false);
	taskEXIT_CRITICAL(&tx_spinlock);
	/* wait for completion */
	for(ch=DO_RMT_CH; ch<=D1_RMT_CH; ch++)
		ESP_ERROR_CHECK(rmt_wait_tx_done(ch, portMAX_DELAY));
}

/* blocks until completion, result [mV] */
uint32_t board_supply_meas(void)
{
	esp_err_t ret;
	adc_digi_output_data_t buf[ADC_READ_CNT];
	uint32_t sample_cnt;
	uint32_t sample_burst;
	uint32_t out_len;
	int sum;
	int cal_out;
	bool done;

	ESP_ERROR_CHECK(adc_digi_start());
	sample_cnt = 0;
	sum = 0;
	done = false;
	while(!done)
	{
		ret = adc_digi_read_bytes((uint8_t *)buf, sizeof(buf), &out_len, ADC_MAX_DELAY);
		if(ret == ESP_OK)
		{
			sample_burst = out_len/sizeof(adc_digi_output_data_t);
			while(sample_burst--)
			{
				sum += buf[sample_burst].type1.data;
				sample_cnt++;
				if(sample_cnt >= VM_AVG_CNT)
				{
					done = true;
					break;
				}
			}
		}
	}
	ESP_ERROR_CHECK(adc_digi_stop());
	sum /= VM_AVG_CNT;
	cal_out = esp_adc_cal_raw_to_voltage(sum, &adc1_chars);
	return((cal_out*VM_GAIN)>>10);
}
