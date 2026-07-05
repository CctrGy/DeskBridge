#include <Arduino.h>

#include <DeskBridgeCore.hpp>

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
