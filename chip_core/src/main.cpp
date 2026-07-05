#include <Arduino.h>

#include <Antenna.hpp>
#include <DeskBridgeCore.hpp>

#if defined(ARDUINO_ARCH_ESP32)

namespace
{
    HardwareTask hardware;
    bool interfaceTaskStarted = false;

    void interfaceTask(void *)
    {
        while (true)
        {
            hardware.updateInterface();
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
}

void setup()
{
    hardware.begin();
    interfaceTaskStarted = xTaskCreatePinnedToCore(interfaceTask, "desk_ui", 6144, nullptr, 1, nullptr, 1) == pdPASS;
}

void loop()
{
    hardware.updateBackend();
    if (!interfaceTaskStarted)
    {
        hardware.updateInterface();
    }
    delay(1);
}

#else

int main()
{
    init();
    initVariant();

    HardwareTask hardware;
    hardware.begin();

    while (1)
    {
        hardware.update();
        delay(1);
    }

    return 0;
}

#endif
