#pragma once

#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>

namespace DeskStorage
{
    template <typename T>
    class EStore
    {
    public:
        using ReadByte = uint8_t (*)(uint32_t address);
        using WriteByte = bool (*)(uint32_t address, uint8_t value);

        EStore(uint32_t address, T *target, ReadByte readByte, WriteByte writeByte)
            : address_(address), target_(target), readByte_(readByte), writeByte_(writeByte)
        {
        }

        bool load()
        {
            if (target_ == nullptr || readByte_ == nullptr)
            {
                return false;
            }

            uint8_t *data = reinterpret_cast<uint8_t *>(target_);
            for (size_t index = 0; index < sizeof(T); ++index)
            {
                data[index] = readByte_(address_ + index);
            }

            return true;
        }

        bool save()
        {
            if (target_ == nullptr || writeByte_ == nullptr)
            {
                return false;
            }

            const uint8_t *data = reinterpret_cast<const uint8_t *>(target_);
            for (size_t index = 0; index < sizeof(T); ++index)
            {
                if (readByte_ != nullptr && readByte_(address_ + index) == data[index])
                {
                    continue;
                }

                if (!writeByte_(address_ + index, data[index]))
                {
                    return false;
                }
            }

            return true;
        }

        T *value()
        {
            return target_;
        }

        const T *value() const
        {
            return target_;
        }

    private:
        uint32_t address_;
        T *target_;
        ReadByte readByte_;
        WriteByte writeByte_;
    };
}
