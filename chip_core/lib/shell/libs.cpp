#include "libs.h"

#include <stdio.h>
#include <string.h>

#include "commands.h"

namespace
{
    const char *stripProtocolPrefix(const char *line)
    {
        if (strncmp(line, "[SHL]", 5) == 0 || strncmp(line, "[CMD]", 5) == 0 ||
            strncmp(line, "[CNF]", 5) == 0 || strncmp(line, "[PRG]", 5) == 0)
        {
            line += 5;
            while (*line == ' ')
            {
                ++line;
            }
        }
        return line;
    }
}

void Shell::begin(ReadFn readFn, WriteFn writeFn)
{
    readFn_ = readFn;
    writeFn_ = writeFn;
    resetLine();
    ready_ = readFn_ != nullptr && writeFn_ != nullptr;

    if (ready_)
    {
        writeLine("[INF] DeskBridge shell ready");
        writeLine("[MRK] 0xA001 shell.begin");
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
        writeLine("[ERR] 0xE001 line too long");
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

    const char *commandLine = stripProtocolPrefix(line_);
    if (!DeskShellCommands::handle(commandLine, *this))
    {
        char response[Parser::MAX_RESPONSE_SIZE] = {};
        parser_.processCommand(commandLine, response);

        if (response[0] != '\0')
        {
            char wrapped[Parser::MAX_RESPONSE_SIZE + 8] = {};
            snprintf(wrapped, sizeof(wrapped), "[RSP] %s", response);
            writeLine(wrapped);
        }
        else
        {
            writeLine("[ERR] 0xE002 unknown command");
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
