#pragma once

#include <stddef.h>
#include <stdint.h>

#include "usb/DeskUSB.h"

class USBCom
{
public:
    bool connected() const
    {
        return DeskUSB::cdcConnected(DeskUSB::CONTROL);
    }

    uint32_t available() const
    {
        return DeskUSB::controlAvailable();
    }

    uint32_t read(void *buffer, uint32_t length)
    {
        return DeskUSB::controlRead(buffer, length);
    }

    uint32_t write(const void *buffer, uint32_t length)
    {
        return DeskUSB::controlWrite(buffer, length);
    }

    void print(const char *text)
    {
        DeskUSB::controlPrint(text);
    }

    void println(const char *text)
    {
        DeskUSB::controlPrintln(text);
    }
};

class USBKeyboard
{
public:
    bool ready() const
    {
        return DeskUSB::hidReady();
    }

    void mute()
    {
        DeskUSB::mediaMute();
    }

    void press(uint8_t modifier, uint8_t keycode)
    {
        DeskUSB::keyboardPress(modifier, keycode);
    }

    void release()
    {
        DeskUSB::keyboardRelease();
    }
};

class USBClass
{
public:
    void begin()
    {
        DeskUSB::begin();
    }

    void task()
    {
        DeskUSB::task();
    }

    bool mounted() const
    {
        return DeskUSB::mounted();
    }

    bool vbusPresent() const
    {
        return DeskUSB::vbusPresent();
    }

    USBCom &getCom()
    {
        return com_;
    }

    USBKeyboard &getKeyboard()
    {
        return keyboard_;
    }

    void log(const char *text)
    {
        DeskUSB::log(text);
    }

    void logln(const char *text)
    {
        DeskUSB::logln(text);
    }

private:
    USBCom com_;
    USBKeyboard keyboard_;
};

extern USBClass USB;
