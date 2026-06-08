#include "led.h"
#include "Pico_WS2812/WS2812.hpp"
#include "tusb.h"
#include "bsp/board_api.h"

#define LED_GPIO 16
static WS2812 ledStrip;

static uint32_t color = WS2812::RGB(0, 0, 0);
static uint32_t led_off_time = 0;
extern "C" {
    uint32_t get_led() {
        return color;
    }
    void led_init(){
        ledStrip.begin(
            LED_GPIO,
         1,
         pio0,
         0,
         WS2812::FORMAT_GRB
        );
    }

    void set_led(uint32_t color) {
        ::color = color;
        ::led_off_time = board_millis() + LED_DEFAULT_TIMEOUT;
        ledStrip.fill(color);
        ledStrip.show();
    }

    void led_green() {
        set_led(WS2812::RGB(0, 5, 0));
    }
    void led_red() {
        set_led(WS2812::RGB(5, 0, 0));
    }
    void led_yellow() {
        set_led(WS2812::RGB(5, 5, 0));
    }
    void led_magenta() {
        set_led(WS2812::RGB(5, 5, 5));
    }
    void led_blue() {
        set_led(WS2812::RGB(0, 0, 5));
    }
    void led_off() {
        set_led(WS2812::RGB(0, 0, 0));
    }
    void led_tick() {
        if(color) {
            uint32_t time = board_millis();
            if(time > ::led_off_time) {
                led_off();
            }
        }
    }
}
