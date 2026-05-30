#include "libs.h"

#include <string.h>

#include "commands.h"

void Shell::begin(ReadFn readFn, WriteFn writeFn)
{
    readFn_ = readFn;
    writeFn_ = writeFn;
    resetLine();
    ready_ = readFn_ != nullptr && writeFn_ != nullptr;

    if (ready_)
    {
        writeLine("DeskBridge shell ready");
        writePrompt();
    }
}

void Shell::update()
{
    if (!ready_)
    {
        return;
    }

    uint8_t buffer[32];
    const uint32_t count = readFn_(buffer, sizeof(buffer));
    for (uint32_t index = 0; index < count; ++index)
    {
        processByte(static_cast<char>(buffer[index]));
    }
}

bool Shell::ready() const
{
    return ready_;
}

void Shell::write(const char *text)
{
    if (!writeFn_ || !text)
    {
        return;
    }

    writeFn_(text, strlen(text));
}

void Shell::writeLine(const char *text)
{
    write(text);
    writeFn_("\r\n", 2);
}

void Shell::processByte(char value)
{
    if (value == '\r')
    {
        return;
    }

    if (value == '\n')
    {
        processLine();
        return;
    }

    if (value == '\b' || value == 0x7F)
    {
        if (lineLength_ > 0)
        {
            --lineLength_;
            line_[lineLength_] = '\0';
        }
        return;
    }

    if (lineLength_ >= sizeof(line_) - 1)
    {
        writeLine("ERR line too long");
        resetLine();
        writePrompt();
        return;
    }

    line_[lineLength_++] = value;
    line_[lineLength_] = '\0';
}

void Shell::processLine()
{
    if (lineLength_ == 0)
    {
        writePrompt();
        return;
    }

    if (!DeskShellCommands::handle(line_, *this))
    {
        char response[Parser::MAX_RESPONSE_SIZE] = {};
        parser_.processCommand(line_, response);

        if (response[0] != '\0')
        {
            writeLine(response);
        }
        else
        {
            writeLine("ERR unknown command");
        }
    }

    resetLine();
    writePrompt();
}

void Shell::resetLine()
{
    lineLength_ = 0;
    line_[0] = '\0';
}

void Shell::writePrompt()
{
    if (!writeFn_)
    {
        return;
    }

    writeFn_("db> ", 4);
}
