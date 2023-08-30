/* led pattern notification defines */
#define LED_NOTIFY_ACCESS_SLOT_1 BIT(0)
#define LED_NOTIFY_ACCESS_SLOT_2 BIT(1)
#define LED_NOTIFY_ACCESS_SLOT_3 BIT(2)
#define LED_NOTIFY_IDLE BIT(26)
#define LED_NOTIFY_NO_CONF BIT(27)
#define LED_NOTIFY_NO_CLOUD BIT(28)
#define LED_NOTIFY_NO_WIFI BIT(29)
#define LED_NOTIFY_ACCESS_DENIED BIT(30)
#define LED_NOTIFY_ACCESS_NO_PRIV BIT(31)
#define LED_MAX_LED_BRG 256
#define LED_SLOT_SEL_PERIOD pdMS_TO_TICKS(5000)
#define LED_IDLE_CHECK_PERIOD pdMS_TO_TICKS(500)
#define LED_ACCESS_DENIED_PERIOD pdMS_TO_TICKS(1000)

typedef enum
{
	LED_ITEM_R,
	LED_ITEM_G,
	LED_ITEM_B,
    LED_ITEM_Y,
    LED_ITEM_MAX
} led_item_t;

void led_start(void);
void led_brightness_set(uint32_t brightness);
void led_task_notify(uint32_t ulValuePattern);
void led_pattern_access_denied(void);
void led_pattern_access_key_dist(void);
void led_pattern_access_no_priv(void);