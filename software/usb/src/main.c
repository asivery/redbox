#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/bootrom.h"
#include "hardware/clocks.h"
#include "hardware/i2c.h"
#include "tusb.h"
#include "bsp/board_api.h"
#include "include.h"
#include "led.h"

#include <stdbool.h>
#include <stddef.h>

#define PIN_HV      0
#define PIN_PGCTRL  1
#define PIN_SDA_PU  4
#define PIN_SCL_PU  5
#define PIN_PWR     13
#define PIN_SDA     10
#define PIN_SCL     11

#define DELAY_PROLOGUE 20
#define DELAY_BLANK 8

#define MAIN_I2C (i2c1)

bool system_configured = false;
bool system_should_program = false;
bool system_should_read = false;
bool system_should_send = false;
bool system_should_read_nopwr = false;
#define I2CPU_MASK (1 << PIN_SDA_PU) | (1 << PIN_SCL_PU)
#define GPIO_ALL_MASK (1 << PIN_HV) | (1 << PIN_PWR) | (1 << PIN_PGCTRL) | I2CPU_MASK

struct  __attribute__((packed)) SystemState {
    uint16_t program_data_length;
    uint16_t checksum;
    uint8_t initseq;
    uint8_t pad;
    uint8_t i2caddrs[0x10];

    uint8_t data[16768];
} payload;

static int errorLine = -1;

static inline void gpio_drive_all(uint32_t mask, uint32_t value) {
    if(value) value = 0xFFFFFFFF;
    gpio_put_masked(mask & ~(1 << PIN_PWR), value);
    // PNP transistor - can't be assed to negate the line in hardware
    gpio_put_masked(mask & (1 << PIN_PWR), ~value);

}
void error_clear_pins() {
    gpio_init_mask(GPIO_ALL_MASK);
    gpio_set_dir_out_masked(GPIO_ALL_MASK);
    gpio_drive_all(GPIO_ALL_MASK, 0);
}

uint8_t locate_i2c_address() {
    uint8_t i2caddr;
    for(int i = 0; i<sizeof(payload.i2caddrs); i++) {
        i2caddr = payload.i2caddrs[i];
        uint8_t data = 0x02;
        if(i2c_write_blocking(MAIN_I2C, i2caddr, &data, 1, false) != PICO_ERROR_GENERIC) {
            return i2caddr;
        }
    }
    return 0xFF;
}

#define CLEAR_ERROR errorLine = 0
#define MARK_ERROR { errorLine = __LINE__; led_red(); }
#define ASSERT_NOT_RV(x, rv) if(x) { printf("Error at %s:%d\n", __FILE__, __LINE__); error_clear_pins(); MARK_ERROR; return rv; }
#define ASSERT_NOT(x) if(x) { printf("Error at %s:%d\n", __FILE__, __LINE__); error_clear_pins(); MARK_ERROR; return; }
#define CHECKING_ERROR_GENERIC(x) ASSERT_NOT ((x) == PICO_ERROR_GENERIC)

void send_init_prologue_zero() {
    // Zero
    sleep_ms(60);
    led_blue();
    gpio_drive_all(GPIO_ALL_MASK, 0);
    sleep_ms(60);

    // Part 1:
    gpio_drive_all(I2CPU_MASK | (1 << PIN_PWR), 1);
    sleep_ms(11);
    gpio_put(PIN_HV, 1);
    sleep_ms(5);
    gpio_drive_all(GPIO_ALL_MASK, 0);
    sleep_ms(60);

    // Part 2:
    gpio_drive_all(I2CPU_MASK | (1 << PIN_PWR) | (1 << PIN_PGCTRL), 1);
    sleep_ms(11);
    gpio_put(PIN_HV, 1);
    sleep_ms(5);
    gpio_drive_all(GPIO_ALL_MASK, 0);
    sleep_ms(60);
    // Part 3:
    gpio_drive_all(I2CPU_MASK | (1 << PIN_PWR) | (1 << PIN_PGCTRL), 1);
    sleep_ms(11);
    gpio_put(PIN_HV, 1);
    sleep_ms(4);
    gpio_put(PIN_PGCTRL, 1);
    sleep_ms(3);
    gpio_drive_all(GPIO_ALL_MASK, 0);
    sleep_ms(60);
    // Part 4:
    gpio_drive_all(1 << PIN_SDA_PU | (1 << PIN_PWR), 1);
    sleep_ms(11);
    gpio_put(PIN_HV, 1);
    sleep_ms(5);
    gpio_drive_all(GPIO_ALL_MASK, 0);
    sleep_ms(60);
    // Part 3 again:
    gpio_drive_all(I2CPU_MASK | (1 << PIN_PGCTRL) | (1 << PIN_PWR), 1);
    sleep_ms(11);
    gpio_put(PIN_HV, 1);
    sleep_ms(4);
    gpio_put(PIN_PGCTRL, 1);
    sleep_ms(3);
    gpio_drive_all(GPIO_ALL_MASK, 0);
    sleep_ms(60);
    // Part 2 (again):
    gpio_drive_all(I2CPU_MASK | (1 << PIN_PGCTRL) | (1 << PIN_PWR), 1);
    sleep_ms(11);
    gpio_put(PIN_HV, 1);
    sleep_ms(5);
    gpio_drive_all(GPIO_ALL_MASK, 0);
    sleep_ms(60);
}

#define MAX_INITSEQ 0
void send_init_prologue() {
    switch(payload.initseq){
        case 0:
            send_init_prologue_zero();
            break;
    }
}

void perform_read() {
    system_should_read = 0;
    gpio_drive_all(GPIO_ALL_MASK, 0);
    led_blue();
    sleep_ms(32);

    send_init_prologue();

    gpio_drive_all(I2CPU_MASK | (1 << PIN_PWR), 1);
    sleep_ms(32);
    gpio_put(PIN_HV, 1);
    led_yellow();
    sleep_ms(32);

    i2c_init(MAIN_I2C, 20*1000);
    uint8_t addr = locate_i2c_address();
    ASSERT_NOT(addr == 0xFF);

    uint8_t start_reg = 0;
    CHECKING_ERROR_GENERIC(i2c_write_blocking(MAIN_I2C, addr, &start_reg, sizeof(start_reg), true));

    int size_to_read = payload.program_data_length;
    if(size_to_read == 0) size_to_read = sizeof(payload.data);
    CHECKING_ERROR_GENERIC(
        payload.program_data_length = i2c_read_blocking(MAIN_I2C, addr, payload.data, size_to_read, false)
    );

    gpio_drive_all(GPIO_ALL_MASK, 0);
    memset(payload.i2caddrs, addr, sizeof(payload.i2caddrs));
    payload.checksum = 0;
    for(int i = 0; i<sizeof(payload.data); i++) payload.checksum += payload.data[i];
    payload.checksum &= ~1;
    printf("Read back %d bytes from the chip. Address is %x. Checksum is %x\n", payload.program_data_length, payload.i2caddrs[0], payload.checksum);
    led_green();
    CLEAR_ERROR;
}

void perform_simple_send() {
    system_should_send = false;
    // Zero is still valid for reads.
    ASSERT_NOT(payload.program_data_length == 0);

    gpio_drive_all(GPIO_ALL_MASK, 0);
    led_blue();
    sleep_ms(20);
    led_yellow();

    sleep_ms(10);
    // 1st bit of checksum = should engage programming pin.
    gpio_drive_all(I2CPU_MASK | (1 << PIN_PWR), 1);

    sleep_ms(4);
    i2c_init(MAIN_I2C, 20*1000);
    sleep_ms(32);


    sleep_ms(32);

    uint8_t addr = locate_i2c_address();
    ASSERT_NOT(addr == 0xFF);

    sleep_ms(12);

    i2c_write_blocking(MAIN_I2C, addr, payload.data, payload.program_data_length, false);

    sleep_ms(32);

    // Leave the power enabled.
    led_green();
    CLEAR_ERROR;
}

void perform_simple_read() {
    system_should_read_nopwr = false;

    sleep_ms(20);
    led_yellow();

    sleep_ms(10);
    // 1st bit of checksum = should engage programming pin.
    gpio_drive_all(I2CPU_MASK | (1 << PIN_PWR), 1);

    sleep_ms(4);
    i2c_init(MAIN_I2C, 20*1000);
    sleep_ms(32);


    sleep_ms(32);

    uint8_t addr = locate_i2c_address();
    ASSERT_NOT(addr == 0xFF);

    sleep_ms(12);

    i2c_write_blocking(MAIN_I2C, addr, payload.data, payload.program_data_length, false);

    sleep_ms(32);

    uint8_t start_reg = 0;
    CHECKING_ERROR_GENERIC(i2c_write_blocking(MAIN_I2C, addr, &start_reg, sizeof(start_reg), true));

    int size_to_read = payload.program_data_length;
    if(size_to_read == 0) size_to_read = sizeof(payload.data);
    CHECKING_ERROR_GENERIC(
        payload.program_data_length = i2c_read_blocking(MAIN_I2C, addr, payload.data, size_to_read, false)
    );

    gpio_drive_all(GPIO_ALL_MASK, 0);
    memset(payload.i2caddrs, addr, sizeof(payload.i2caddrs));
    payload.checksum = 0;
    for(int i = 0; i<sizeof(payload.data); i++) payload.checksum += payload.data[i];
    payload.checksum &= ~1;
    printf("Read back %d bytes from the chip (without reset). Address is %x. Checksum is %x\n", payload.program_data_length, payload.i2caddrs[0], payload.checksum);
    led_green();
    CLEAR_ERROR;
}

void perform_program() {
    system_should_program = false;
    // Zero is still valid for reads.
    ASSERT_NOT(payload.program_data_length == 0);

    gpio_drive_all(GPIO_ALL_MASK, 0);
    led_blue();
    sleep_ms(20);

    send_init_prologue();

    led_yellow();

    sleep_ms(10);
    // 1st bit of checksum = should engage programming pin.
    gpio_put(PIN_PGCTRL, payload.checksum & 1);
    gpio_drive_all(I2CPU_MASK | (1 << PIN_PWR), 1);

    sleep_ms(4);
    i2c_init(MAIN_I2C, 20*1000);
    sleep_ms(32);


    gpio_put(PIN_HV, 1);
    sleep_ms(32);

    uint8_t addr = locate_i2c_address();
    ASSERT_NOT(addr == 0xFF);

    sleep_ms(12);

    i2c_write_blocking(MAIN_I2C, addr, payload.data, payload.program_data_length, false);
    
    sleep_ms(32);

    gpio_put(PIN_HV, 0);
    sleep_ms(32);

    gpio_drive_all(GPIO_ALL_MASK, 0);

    led_green();
    CLEAR_ERROR;
}

void main(void) {
    uint32_t i2cmask = (1 << PIN_SCL) | (1 << PIN_SDA);
    set_sys_clock_khz(240000, 1);
    board_init();
    tusb_init();
    board_init_after_tusb();
    led_init();
    led_magenta();
    stdio_init_all();
    gpio_set_function_masked(i2cmask, GPIO_FUNC_I2C);

    gpio_init_mask(GPIO_ALL_MASK);
    gpio_set_dir_out_masked(GPIO_ALL_MASK);
    gpio_drive_all(GPIO_ALL_MASK, 0);

    sleep_ms(10);



    while (true) {
        tud_task();
        led_tick();

        if(system_should_program) {
            perform_program();
        }
        if(system_should_read) {
            perform_read();
        }
        if(system_should_send){
            perform_simple_send();
        }
        if(system_should_read_nopwr){
            perform_simple_read();
        }
    }
}

// Vendor code:

uint32_t bulk_write_offset = 0;
uint32_t bulk_read_offset = 0;

void tud_vendor_rx_cb(uint8_t itf, uint8_t const* buffer, uint16_t bufsize) {
    if(itf == ITF_NUM_VENDOR) {
        if((bulk_write_offset + bufsize) > sizeof(payload)) {
            MARK_ERROR;
            printf("Too much data!\n");
        } else {
            memcpy(((uint8_t *) &payload) + bulk_write_offset, buffer, bufsize);
            printf("Payload[%x..%x] written.\n", bulk_write_offset, bulk_write_offset + bufsize);
            bulk_write_offset += bufsize;
        }
    }
#if CFG_TUD_VENDOR_RX_BUFSIZE > 0
    tud_vendor_read_flush();
#endif
    return;
}

void tud_vendor_tx_cb(uint8_t itf, uint32_t sent_bytes) {
    bulk_read_offset += sent_bytes;
    int bytes_remaining = offsetof(struct SystemState, data) + payload.program_data_length - bulk_read_offset;
    if(bytes_remaining > 64) bytes_remaining = 64;
    if(bytes_remaining > 0) {
        tud_vendor_write(((uint8_t *)&payload) + bulk_read_offset, bytes_remaining);
    }
}


bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const* request) {
    union {
        uint8_t rawByte;
        uint32_t errorLine;
        uint32_t length;
    } usbData;
    static uint8_t responseLength;

    // Used to test the direction
    bool isInbound = (request->bmRequestType_bit.direction == TUSB_DIR_IN) ? true : false; 

    if (request->bmRequestType_bit.type != TUSB_REQ_TYPE_CLASS || request->wIndex != ITF_NUM_VENDOR) {
        return false;
    }

    switch (stage) {
        case CONTROL_STAGE_SETUP:
            memset(&usbData, 0, sizeof(usbData));

            // Handle the request
            switch (request->bRequest) {
                case 1:
                    // OUT: Program - trigger the flashing of the GP. - out
                    if (isInbound) {
                        MARK_ERROR;
                        return false;
                    }

                    if (!system_configured) {
                        MARK_ERROR;
                    } else {
                        system_should_program = true;
                        errorLine = -1;
                    }

                    responseLength = 1;
                    break;
                case 2:
                    // OUT: Reset
                    if (isInbound) {
                        MARK_ERROR;
                        return false;
                    }
                    bulk_write_offset = 0;
                    errorLine = -1;
                    break;
                case 3:
                    // OUT: Validate and reset
                    {
                        if (isInbound) {
                            return false;
                        }

                        ASSERT_NOT_RV(payload.initseq > MAX_INITSEQ, false);

                        uint16_t sum = 0;
                        memset(payload.data + payload.program_data_length, 0, sizeof(payload.data) - payload.program_data_length);
                        for(int i = 0; i<sizeof(payload.data); i++)
                            sum += payload.data[i];
                        sum &= ~1;
                        if(sum != (payload.checksum & ~1)) {
                            printf("Checksum err: %x != %x\n", sum, payload.checksum & ~1);
                            MARK_ERROR;
                        } else {
                            printf("Checksum OK. System primed\n");
                            system_configured = true;
                            CLEAR_ERROR;
                        }
                        bulk_write_offset = 0;
                    }
                    break;
                case 4:
                    // OUT: Trigger read
                    if(isInbound) {
                        MARK_ERROR;
                        return false;
                    }

                    if (!system_configured) {
                        MARK_ERROR;
                    } else {
                        system_should_read = true;
                        errorLine = -1;
                    }
                    break;
                case 5:
                    // OUT: Just write the data over i2c
                    if (isInbound) {
                        MARK_ERROR;
                        return false;
                    }

                    if (!system_configured) {
                        MARK_ERROR;
                    } else {
                        system_should_send = true;
                        errorLine = -1;
                    }

                    responseLength = 1;
                    break;
                case 6:
                    // OUT: Read data from I2c without resetting
                    if (isInbound) {
                        MARK_ERROR;
                        return false;
                    }

                    if (!system_configured) {
                        MARK_ERROR;
                    } else {
                        system_should_read_nopwr = true;
                        errorLine = -1;
                    }

                    responseLength = 1;
                    break;
                case 0xFC:
                    // IN: Read header
                    if(!isInbound) return false;
                    usbData.length = offsetof(struct SystemState, data) + payload.program_data_length;
                    responseLength = sizeof(usbData.length);
                    break;
                case 0xFD:
                    // In: Read program data
                    if(!isInbound) return false;
                    responseLength = 1;
                    bulk_read_offset = 0;
                    tud_vendor_write(&payload, CFG_TUD_CDC_TX_BUFSIZE);
                    break;
                case 0xFE:
                    // IN: Read error
                    if(!isInbound) return false;
                    responseLength = sizeof(errorLine);
                    usbData.errorLine = (uint32_t) errorLine;
                    break;
                default:
                    return false;
            }

            return tud_control_xfer(rhport, request, &usbData, responseLength);
        case CONTROL_STAGE_DATA:
            // The buffer is filled - from PC to dev.

        default:
            return true;
    }
}
