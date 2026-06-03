#include "usb/usb_descriptors.h"

#include <string.h>

#include "core/DeskBridgeVersion.h"

enum
{
    REPORT_ID_KEYBOARD = 1,
    REPORT_ID_CONSUMER_CONTROL,
};

static tusb_desc_device_t const desc_device = {
    sizeof(tusb_desc_device_t),
    TUSB_DESC_DEVICE,
    0x0200,
    TUSB_CLASS_MISC,
    MISC_SUBCLASS_COMMON,
    MISC_PROTOCOL_IAD,
    CFG_TUD_ENDPOINT0_SIZE,
    DESKBRIDGE_USB_VID,
    DESKBRIDGE_USB_PID,
    DeskBridgeVersion::USB_DEVICE_BCD,
    STRID_MANUFACTURER,
    STRID_PRODUCT,
    STRID_SERIAL,
    1,
};

uint8_t const *tud_descriptor_device_cb(void)
{
    return reinterpret_cast<uint8_t const *>(&desc_device);
}

static uint8_t const desc_hid_report[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(REPORT_ID_KEYBOARD)),
    TUD_HID_REPORT_DESC_CONSUMER(HID_REPORT_ID(REPORT_ID_CONSUMER_CONTROL)),
};

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    (void)instance;
    return desc_hid_report;
}

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + TUD_HID_DESC_LEN)

static uint8_t const desc_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0, 100),
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC0, STRID_CDC_CONTROL, EPNUM_CDC0_NOTIF, 8,
                       EPNUM_CDC0_OUT, EPNUM_CDC0_IN, 64),
    TUD_HID_DESCRIPTOR(ITF_NUM_HID, STRID_HID, HID_ITF_PROTOCOL_NONE,
                       sizeof(desc_hid_report), EPNUM_HID_IN,
                       CFG_TUD_HID_EP_BUFSIZE, 10),
};

uint8_t const *tud_descriptor_configuration_cb(uint8_t index)
{
    (void)index;
    return desc_configuration;
}

static char const *const desc_strings[] = {
    nullptr,
    "CctrGy",
    "DeskBridge Control Hub",
    "DB01-000001",
    "DeskBridge SerialUSB",
    "DeskBridge HID",
};

static uint16_t string_descriptor[32];

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    (void)langid;

    uint8_t character_count = 0;
    if (index == STRID_LANGID)
    {
        string_descriptor[1] = 0x0409;
        character_count = 1;
    }
    else
    {
        if (index >= sizeof(desc_strings) / sizeof(desc_strings[0]))
        {
            return nullptr;
        }

        char const *source = desc_strings[index];
        character_count = static_cast<uint8_t>(strlen(source));
        if (character_count > 31)
        {
            character_count = 31;
        }

        for (uint8_t i = 0; i < character_count; ++i)
        {
            string_descriptor[1 + i] = source[i];
        }
    }

    string_descriptor[0] =
        static_cast<uint16_t>((TUSB_DESC_STRING << 8) | (2 * character_count + 2));
    return string_descriptor;
}
