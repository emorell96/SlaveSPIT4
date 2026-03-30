#include "Arduino.h"

namespace SlaveSpi
{
    uint16_t crc16_ccitt_false(const uint8_t* data, std::size_t length)
    {
        uint16_t crc = 0xFFFF;

        for (std::size_t i = 0; i < length; ++i)
        {
            crc ^= static_cast<uint16_t>(data[i]) << 8;

            for (int j = 0; j < 8; ++j)
            {
                if (crc & 0x8000)
                    crc = static_cast<uint16_t>((crc << 1) ^ 0x1021);
                else
                    crc = static_cast<uint16_t>(crc << 1);
            }
        }

        return crc;
    }

    uint16_t crc16_words(const uint16_t* data, std::size_t wordCount)
    {
        return crc16_ccitt_false( // CRC is computed over the raw byte representation of the 16-bit word buffer in native Teensy byte order.
            reinterpret_cast<const uint8_t*>(data),
            wordCount * sizeof(uint16_t)
        );
    }
}