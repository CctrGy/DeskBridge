#include <Arduino.h>

#if defined(ARDUINO_ARCH_STM32)
#include <PeripheralPins.h>

#if defined(HAL_PCD_MODULE_ENABLED) || defined(HAL_HCD_MODULE_ENABLED)
const PinMap PinMap_USB_OTG_FS[] = {
    {PA_11, USB_OTG_FS, STM_PIN_DATA(STM_MODE_AF_PP, GPIO_PULLUP, GPIO_AF10_OTG_FS)},
    {PA_12, USB_OTG_FS, STM_PIN_DATA(STM_MODE_AF_PP, GPIO_PULLUP, GPIO_AF10_OTG_FS)},
    {NC, NP, 0}
};
#endif

#endif
