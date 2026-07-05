#include "libs.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "core/ErrorCodes.h"
#include "commands.h"

namespace
{
    bool equalsHeader(const char *left, const char *right)
    {
        while (*left != '\0' && *right != '\0')
        {
            if (tolower(static_cast<unsigned char>(*left)) != tolower(static_cast<unsigned char>(*right)))
            {
                return false;
            }
            ++left;
            ++right;
        }
        return *left == '\0' && *right == '\0';
    }

    bool isInputProtocolHeader(const char *header)
    {
        return equalsHeader(header, "SHL") ||
               equalsHeader(header, "CMD") ||
               equalsHeader(header, "DAT") ||
               equalsHeader(header, "STK") ||
               equalsHeader(header, "STE") ||
               equalsHeader(header, "INF") ||
               equalsHeader(header, "CNF") ||
               equalsHeader(header, "PRG");
    }

    const char *stripProtocolPrefix(const char *line)
    {
        if (line[0] == '[' && line[4] == ']')
        {
            char header[4] = {line[1], line[2], line[3], '\0'};
            if (isInputProtocolHeader(header))
            {
                line += 5;
                while (*line == ' ')
                {
                    ++line;
                }
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
        writeLine("[MRK] 0x0000A001 shell.begin");
        writeLine("[LOG] shell: ready");
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

void Shell::writeAsyncLine(const char *text)
{
    if (!writeFn_ || !text)
    {
        return;
    }

    writeFn_("\r\n", 2);
    writeLine(text);
    writePrompt();
}

void Shell::writeTuiNotification(const char *code, const char *summary, const char *detail)
{
    char line[192] = {};
    snprintf(line,
             sizeof(line),
             "[TFI] %s %s | %s",
             code && code[0] != '\0' ? code : "0x00000000",
             summary && summary[0] != '\0' ? summary : "notification",
             detail && detail[0] != '\0' ? detail : summary && summary[0] != '\0' ? summary : "notification");
    writeLine(line);
}

void Shell::writeAsyncTuiNotification(const char *code, const char *summary, const char *detail)
{
    char line[192] = {};
    snprintf(line,
             sizeof(line),
             "[TFI] %s %s | %s",
             code && code[0] != '\0' ? code : "0x00000000",
             summary && summary[0] != '\0' ? summary : "notification",
             detail && detail[0] != '\0' ? detail : summary && summary[0] != '\0' ? summary : "notification");
    writeAsyncLine(line);
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
        char error[64] = {};
        snprintf(error, sizeof(error), "[ERR] 0x%08lX line too long", static_cast<unsigned long>(DeskError::ShellLineTooLong));
        writeLine(error);
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
            char error[64] = {};
            snprintf(error, sizeof(error), "[ERR] 0x%08lX unknown command", static_cast<unsigned long>(DeskError::ShellUnknownCommand));
            writeLine(error);
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
