//
// Copyright (c) 2025 Piers Finlayson <piers@piers.rocks>
//
// Licensed under MIT license - see https://opensource.org/licenses/MIT
//
#include <stdint.h>

// Maximum packet sizes for the endpoints.  64 is a very standard value
#define MAX_ENDPOINT0_SIZE  64
#define ENDPOINT_BULK_SIZE  64

// Indexes for the strings in the USB device descriptor
enum {
    STRID_LANGID = 0,
    STRID_MANUFACTURER,
    STRID_PRODUCT,
    STRID_SERIAL,
};

// Interfaces for the USB device descriptor
enum {
    ITF_NUM_VENDOR = 0,
    ITF_NUM_CDC_DATA,
    ITF_NUM_CDC,
    ITF_NUM_TOTAL
};

// These could be 0x01 and 0x02 (with the IN ORed with 0x80).  I'm setting
// them to 0x83 and 0x04 to replicate another device.
#define ENDPOINT_isInbound 0x80
#define BULK_IN_ENDPOINT_DIR   (0x3 | ENDPOINT_isInbound)
#define BULK_OUT_ENDPOINT_DIR   (0x4)

#define EP_CDC_0_IN   (0x8 | ENDPOINT_isInbound)
#define EP_CDC_0_OUT   (0x8)
#define EP_CDC_0_NOTIF   (0x7 | ENDPOINT_isInbound)

