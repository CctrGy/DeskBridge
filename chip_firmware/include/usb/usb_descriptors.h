#pragma once

#include <stdint.h>
#include "tusb.h"

// USB identity ---------------------------------------------------------------

#ifndef DESKBRIDGE_USB_VID
  #define DESKBRIDGE_USB_VID 0x1209
#endif

#ifndef DESKBRIDGE_USB_PID
  #define DESKBRIDGE_USB_PID 0xDB01
#endif

// Interface layout -----------------------------------------------------------
// Each CDC ACM consumes 2 interfaces: Communication + Data.
// HID consumes 1 interface. DeskBridge currently exposes one physical CDC ACM
// function together with HID, then multiplexes logical channels in CDC0.

#define ITF_NUM_CDC0         0
#define ITF_NUM_CDC0_DATA    1
#define ITF_NUM_HID          2
#define ITF_NUM_TOTAL        3

// CDC instance mapping used by TinyUSB tud_cdc_n_* APIs.
#define DESKUSB_CDC_CONTROL  0   // SerialUSB / PC main program

// Endpoint allocation --------------------------------------------------------
// Conservative STM32F4 layout: endpoint numbers 0..3. Do not add another CDC
// endpoint set here unless the target USB peripheral and descriptors are
// intentionally expanded.

#define EPNUM_CDC0_NOTIF     0x81
#define EPNUM_CDC0_OUT       0x02
#define EPNUM_CDC0_IN        0x82

#define EPNUM_HID_IN         0x83

// String descriptor indexes --------------------------------------------------

enum
{
    STRID_LANGID = 0,
    STRID_MANUFACTURER,
    STRID_PRODUCT,
    STRID_SERIAL,
    STRID_CDC_CONTROL,
    STRID_HID,
};

#ifdef __cplusplus
extern "C" {
#endif

// Implemented in src/usb_descriptors.cpp
uint8_t const * tud_descriptor_device_cb(void);
uint8_t const * tud_descriptor_configuration_cb(uint8_t index);
uint16_t const * tud_descriptor_string_cb(uint8_t index, uint16_t langid);
uint8_t const * tud_hid_descriptor_report_cb(uint8_t instance);

#ifdef __cplusplus
}
#endif
