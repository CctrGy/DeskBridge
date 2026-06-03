#pragma once

/*
  DeskBridge TinyUSB configuration
  Target: STM32F411 BlackPill / USB FS

  STM32F411 OTG FS target:
    - 1 x CDC ACM
    - 1 x HID Consumer/Keyboard

  This project currently keeps the same conservative USB layout used on F401:
  four bidirectional endpoint numbers including EP0.
  One CDC ACM function needs two IN endpoint numbers and one OUT endpoint;
  HID uses the remaining non-control IN endpoint.
*/

#ifndef CFG_TUSB_MCU
  #define CFG_TUSB_MCU OPT_MCU_STM32F4
#endif

#ifndef CFG_TUSB_OS
  #define CFG_TUSB_OS OPT_OS_NONE
#endif

#ifndef CFG_TUSB_RHPORT0_MODE
  #define CFG_TUSB_RHPORT0_MODE (OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED)
#endif

#ifndef CFG_TUSB_DEBUG
  #define CFG_TUSB_DEBUG 0
#endif

// Endpoint 0 control size for full-speed USB.
#ifndef CFG_TUD_ENDPOINT0_SIZE
  #define CFG_TUD_ENDPOINT0_SIZE 64
#endif

// Device classes enabled.
// Conservative endpoint budget: EP0 control, EP1 CDC notification, EP2 CDC data,
// EP3 HID interrupt IN.
#ifndef CFG_TUD_CDC
  #define CFG_TUD_CDC 1
#endif

#ifndef CFG_TUD_HID
  #define CFG_TUD_HID 1
#endif

#define CFG_TUD_MSC     0
#define CFG_TUD_MIDI    0
#define CFG_TUD_VENDOR  0

// CDC buffers.
#define CFG_TUD_CDC_RX_BUFSIZE   256
#define CFG_TUD_CDC_TX_BUFSIZE   256

// HID endpoint buffer.
#define CFG_TUD_HID_EP_BUFSIZE   16
