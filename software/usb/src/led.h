#pragma once
#ifdef __cplusplus
#include <cstdint>
extern "C" {
#else
#include <stdint.h>
#endif

#define LED_DEFAULT_TIMEOUT 500 /* ms */

void led_green();
void led_blue();
void led_yellow();
void led_red();
void led_magenta();
void led_off();
void led_init();
uint32_t get_led();
void set_led(uint32_t color);
void led_tick();

#ifdef __cplusplus
}
#endif
