#pragma once

#include <stdint.h>
#include <stddef.h>

/*
  DeskUSB public API

  STM32F411 implementation target:
    - cdc:     CDC0, one physical USB serial port exposed to the PC
    - control: main bidirectional channel inside CDC0
    - log:     firmware log channel inside CDC0
    - hid:     HID consumer/keyboard utilities

  The .cpp implementation will wrap TinyUSB functions:
    - tud_init()
    - tud_task()
    - tud_cdc_n_available/read/write/flush()
    - tud_hid_report()
*/

namespace DeskUSB
{
    enum CdcPort : uint8_t
    {
        CONTROL = 0,
    };

    void begin();
    void task();
    bool mounted();

    // USB_VBUS reports voltage presence, not whether the host has enumerated
    // DeskBridge successfully.
    bool vbusPresent();

    // USB_ID is active low on OTG-style connectors: grounded means host role,
    // floating/pulled high means device role.
    bool idGrounded();

    // Generic CDC access ------------------------------------------------------
    bool cdcConnected(CdcPort port);
    uint32_t available(CdcPort port);
    uint32_t read(CdcPort port, void *buffer, uint32_t length);
    uint32_t write(CdcPort port, const void *buffer, uint32_t length);
    void flush(CdcPort port);

    // Main control port helpers ---------------------------------------------
    uint32_t controlAvailable();
    uint32_t controlRead(void *buffer, uint32_t length);
    uint32_t controlWrite(const void *buffer, uint32_t length);
    void controlPrint(const char *text);
    void controlPrintln(const char *text);

    // Logs share the physical CDC0 port with control traffic. The PC protocol
    // will frame both logical channels without creating another Windows COM port.
    void log(const char *text);
    void logln(const char *text);

    // HID helpers -------------------------------------------------------------
    bool hidReady();

    // Consumer-control mute key. Best for OS-level mute/unmute.
    void mediaMute();
    void mediaControl(uint16_t usage);

    // Normal keyboard helpers for F13/F14/macros later.
    void keyboardPress(uint8_t modifier, uint8_t keycode);
    void keyboardRelease();
}
