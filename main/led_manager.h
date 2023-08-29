/* notification defines */
#define LED_NOTIFY_NO_CONF BIT(0)
#define LED_NOTIFY_NO_CLOUD BIT(1)
#define LED_NOTIFY_NO_WIFI BIT(2)
#define LED_NOTIFY_ACCESS_DENIED BIT(3)
#define LED_NOTIFY_ACCESS_GRANTED BIT(4)
#define LED_NOTIFY_ACCESS_NO_PRIV BIT(5)
#define LED_NOTIFY_ACCESS_SLOT_SEL BIT(6)
#define LED_NOTIFY_IDLE BIT(7)

typedef enum
{
	LED_ITEM_R,
	LED_ITEM_G,
	LED_ITEM_B,
    LED_ITEM_Y,
} led_item_t;

void led_start(void);
void led_brightness_set(uint32_t brightness);
void led_pattern_idle(void);
void led_task_notify(uint32_t ulValuePattern);