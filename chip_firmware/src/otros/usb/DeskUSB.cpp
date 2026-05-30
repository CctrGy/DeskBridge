#include "usb/DeskUSB.h"

#include <string.h>

#include "stm32f4xx_hal.h"
#include "tusb.h"

namespace
{
    enum HidReportId : uint8_t
    {
        REPORT_ID_KEYBOARD = 1,
        REPORT_ID_CONSUMER_CONTROL,
    };

    void configureUsbFs()
    {
        __HAL_RCC_GPIOA_CLK_ENABLE();

        GPIO_InitTypeDef vbus = {};
        vbus.Pin = GPIO_PIN_9;
        vbus.Mode = GPIO_MODE_INPUT;
        vbus.Pull = GPIO_PULLDOWN;
        HAL_GPIO_Init(GPIOA, &vbus);

        GPIO_InitTypeDef gpio = {};
        gpio.Pin = GPIO_PIN_11 | GPIO_PIN_12;
        gpio.Mode = GPIO_MODE_AF_PP;
        gpio.Pull = GPIO_NOPULL;
        gpio.Speed = GPIO_SPEED_HIGH;
        gpio.Alternate = GPIO_AF10_OTG_FS;
        HAL_GPIO_Init(GPIOA, &gpio);

        __HAL_RCC_USB_OTG_FS_CLK_ENABLE();

        // The BlackPill USB connector is device-only and has no VBUS sense pin.
        USB_OTG_FS->GCCFG |= USB_OTG_GCCFG_NOVBUSSENS;
        USB_OTG_FS->GCCFG &= ~USB_OTG_GCCFG_VBUSBSEN;
        USB_OTG_FS->GCCFG &= ~USB_OTG_GCCFG_VBUSASEN;
    }

}

namespace DeskUSB
{
    void begin()
    {
        configureUsbFs();
        tud_init(0);
    }

    void task()
    {
        tud_task();
    }

    bool mounted()
    {
        return tud_mounted();
    }

    bool vbusPresent()
    {
        return HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_9) == GPIO_PIN_SET;
    }

    bool cdcConnected(CdcPort port)
    {
        return tud_cdc_n_connected(port);
    }

    uint32_t available(CdcPort port)
    {
        return tud_cdc_n_available(port);
    }

    uint32_t read(CdcPort port, void *buffer, uint32_t length)
    {
        return tud_cdc_n_read(port, buffer, length);
    }

    uint32_t write(CdcPort port, const void *buffer, uint32_t length)
    {
        return tud_cdc_n_write(port, buffer, length);
    }

    void flush(CdcPort port)
    {
        tud_cdc_n_write_flush(port);
    }

    uint32_t controlAvailable()
    {
        return available(CONTROL);
    }

    uint32_t controlRead(void *buffer, uint32_t length)
    {
        return read(CONTROL, buffer, length);
    }

    uint32_t controlWrite(const void *buffer, uint32_t length)
    {
        return write(CONTROL, buffer, length);
    }

    void controlPrint(const char *text)
    {
        controlWrite(text, strlen(text));
    }

    void controlPrintln(const char *text)
    {
        controlPrint(text);
        controlPrint("\r\n");
    }

    void log(const char *text)
    {
        controlPrint(text);
        flush(CONTROL);
    }

    void logln(const char *text)
    {
        controlPrintln(text);
        flush(CONTROL);
    }

    bool hidReady()
    {
        return tud_hid_ready();
    }

    void mediaMute()
    {
        if (!hidReady())
        {
            return;
        }

        uint16_t usage = HID_USAGE_CONSUMER_MUTE;
        tud_hid_report(REPORT_ID_CONSUMER_CONTROL, &usage, sizeof(usage));
        tud_task();

        usage = 0;
        tud_hid_report(REPORT_ID_CONSUMER_CONTROL, &usage, sizeof(usage));
    }

    void keyboardPress(uint8_t modifier, uint8_t keycode)
    {
        if (!hidReady())
        {
            return;
        }

        uint8_t keycodes[6] = {keycode, 0, 0, 0, 0, 0};
        tud_hid_keyboard_report(REPORT_ID_KEYBOARD, modifier, keycodes);
    }

    void keyboardRelease()
    {
        if (!hidReady())
        {
            return;
        }

        tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, nullptr);
    }
}

extern "C" uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                                            hid_report_type_t report_type,
                                            uint8_t *buffer, uint16_t reqlen)
{
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)reqlen;
    return 0;
}

extern "C" void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                                        hid_report_type_t report_type,
                                        uint8_t const *buffer, uint16_t bufsize)
{
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)bufsize;
}

extern "C" void OTG_FS_IRQHandler(void)
{
    tud_int_handler(0);
}
