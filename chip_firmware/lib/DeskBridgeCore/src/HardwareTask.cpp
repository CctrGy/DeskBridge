#include "HardwareTask.hpp"

#include "core/DeskBridgeVersion.h"
#include "core/DeskHardware.h"
#include "USB.hpp"

void HardwareTask::begin()
{
    USB.begin();
    DeskHardware::begin();
    heartbeat_.reset();
}

void HardwareTask::update()
{
    loopStartUs_ = micros();
    ++cycles_;

    USB.task();
    DeskHardware::update();

    if (!identityLogged_ && USB.getCom().connected())
    {
        logFirmwareIdentity();
        identityLogged_ = true;
    }

    logHeartbeat();

    lastLoopMicros_ = micros() - loopStartUs_;
    if (lastLoopMicros_ > maxLoopMicros_)
    {
        maxLoopMicros_ = lastLoopMicros_;
    }
}

void HardwareTask::logFirmwareIdentity()
{
    DeskBridgeVersion::Info version = DeskBridgeVersion::current();

    USB.log("DeskBridge firmware ");
    USB.log(version.firmware);
    USB.log(" | serial protocol 0x");

    char protocolHex[] = {
        "0123456789ABCDEF"[version.serialProtocol >> 4],
        "0123456789ABCDEF"[version.serialProtocol & 0x0F],
        '\0',
    };
    USB.log(protocolHex);
    USB.logln("");
}

void HardwareTask::logHeartbeat()
{
    if (heartbeat_.elapsed())
    {
        USB.log("DeskBridge online\r\n");
    }
}
