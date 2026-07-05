#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <CommandParser.h>

class Shell
{
public:
    using Parser = CommandParser<16, 4, 16, 32, 96>;
    using ReadFn = uint32_t (*)(void *buffer, uint32_t length);
    using WriteFn = uint32_t (*)(const void *buffer, uint32_t length);

    void begin(ReadFn readFn, WriteFn writeFn);
    void update();

    bool ready() const;
    void write(const char *text);
    void writeLine(const char *text);

private:
    void processByte(char value);
    void processLine();
    void resetLine();
    void writePrompt();

    Parser parser_;
    ReadFn readFn_ = nullptr;
    WriteFn writeFn_ = nullptr;
    char line_[128] = {};
    uint8_t lineLength_ = 0;
    bool ready_ = false;
};
