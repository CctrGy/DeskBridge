#include "usb/DeskUSB.h"

#include <string.h>

#include "config/pins_config.h"

#if defined(ARDUINO_ARCH_ESP32)

#include "USB.h"
#include "USBCDC.h"
#include "USBHIDConsumerControl.h"
#include "USBHIDKeyboard.h"
#include "core/DeskBridgeVersion.h"
#include "usb/usb_descriptors.h"

namespace
{
    USBCDC deskCdc(0);
#if CONFIG_TINYUSB_HID_ENABLED
    USBHIDConsumerControl deskConsumer;
    USBHIDKeyboard deskKeyboard;
#endif
    bool usbMounted = false;

    void usbEventCallback(void *arg, esp_event_base_t eventBase, int32_t eventId, void *eventData)
    {
        (void)arg;
        (void)eventData;

        if (eventBase == ARDUINO_USB_EVENTS)
        {
            if (eventId == ARDUINO_USB_STARTED_EVENT)
            {
                usbMounted = true;
            }
            else if (eventId == ARDUINO_USB_STOPPED_EVENT)
            {
                usbMounted = false;
            }
        }
    }
}

namespace DeskUSB
{
    void begin()
    {
        ::USB.VID(DESKBRIDGE_USB_VID);
        ::USB.PID(DESKBRIDGE_USB_PID);
        ::USB.manufacturerName("CctrGy");
        ::USB.productName("DeskBridge Control Hub");
        ::USB.serialNumber("DB01-ESP32S3");
        ::USB.firmwareVersion(DeskBridgeVersion::USB_DEVICE_BCD);
        ::USB.usbVersion(0x0200);
        ::USB.usbPower(500);

        ::USB.onEvent(usbEventCallback);
        deskCdc.onEvent(usbEventCallback);
        deskCdc.begin(115200);
#if CONFIG_TINYUSB_HID_ENABLED
        deskConsumer.begin();
        deskKeyboard.begin();
#endif
        ::USB.begin();
    }

    void task()
    {
    }

    bool mounted()
    {
        return usbMounted;
    }

    bool vbusPresent()
    {
        return true;
    }

    bool idGrounded()
    {
        return false;
    }

    bool cdcConnected(CdcPort port)
    {
        (void)port;
        return static_cast<bool>(deskCdc);
    }

    uint32_t available(CdcPort port)
    {
        (void)port;
        return static_cast<uint32_t>(deskCdc.available());
    }

    uint32_t read(CdcPort port, void *buffer, uint32_t length)
    {
        (void)port;
        const int pending = deskCdc.available();
        if (pending <= 0 || length == 0)
        {
            return 0;
        }

        const uint32_t count = pending < static_cast<int>(length) ? static_cast<uint32_t>(pending) : length;
        return static_cast<uint32_t>(deskCdc.read(static_cast<uint8_t *>(buffer), count));
    }

    uint32_t write(CdcPort port, const void *buffer, uint32_t length)
    {
        (void)port;
        return static_cast<uint32_t>(deskCdc.write(static_cast<const uint8_t *>(buffer), length));
    }

    void flush(CdcPort port)
    {
        (void)port;
        deskCdc.flush();
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
        return usbMounted;
    }

    void mediaMute()
    {
        mediaControl(0x00E2);
    }

    void mediaControl(uint16_t usage)
    {
#if CONFIG_TINYUSB_HID_ENABLED
        if (!hidReady())
        {
            return;
        }
        deskConsumer.press(usage);
        deskConsumer.release();
#else
        (void)usage;
#endif
    }

    void keyboardPress(uint8_t modifier, uint8_t keycode)
    {
        (void)modifier;
#if CONFIG_TINYUSB_HID_ENABLED
        if (!hidReady())
        {
            return;
        }
        deskKeyboard.pressRaw(keycode);
#else
        (void)keycode;
#endif
    }

    void keyboardRelease()
    {
#if CONFIG_TINYUSB_HID_ENABLED
        deskKeyboard.releaseAll();
#endif
    }
}

#else

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
        return false;
    }

    bool idGrounded()
    {
        return false;
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
        mediaControl(HID_USAGE_CONSUMER_MUTE);
    }

    void mediaControl(uint16_t usage)
    {
        if (!hidReady())
        {
            return;
        }

        tud_hid_report(REPORT_ID_CONSUMER_CONTROL, &usage, sizeof(usage));
        tud_task();

        uint16_t release = 0;
        tud_hid_report(REPORT_ID_CONSUMER_CONTROL, &release, sizeof(release));
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

#endif
