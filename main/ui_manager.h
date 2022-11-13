#ifndef MAIN_UI_MANAGER_H_
#define MAIN_UI_MANAGER_H_

#define UI_PATTERN_LOOP BIT(31)
/* patterns */
#define UI_NET_OK (UI_PATTERN_LOOP | 0x7fffffff)
#define UI_NET_NO_WIFI (UI_PATTERN_LOOP | 0x3)
#define UI_NET_NO_CLOUD (UI_PATTERN_LOOP | 0x303)
#define UI_ACCESS_GRANTED 0, 0x49, 0x49, true
#define UI_WIEGAND 0, 0x49, 0x49, false
#define UI_ACCESS_DENIED 0x1f1f, 0, 0x1f1f, false
#define UI_SET_WIFI 0x3, 0x3, 0x3, false
#define UI_SET_CLOUD 0x303, 0x303, 0x303, false
#define UI_SET_LED 0x30303, 0x30303, 0x30303, false
/* maximum LED brightness */
#define UI_MAX_LED_BRG 256

void ui_start(void);
void ui_brightness(uint32_t brightness);
void ui_rg_beep_open(uint32_t r, uint32_t g, uint32_t beep, bool open);
void ui_blue(uint32_t b);

#endif /* MAIN_UI_MANAGER_H_ */
