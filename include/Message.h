#if !defined(MESSAGE_H)
#define MESSAGE_H

#include "Arduino.h"
#include "circular_buffer.h"

namespace SlaveSpi
{
    struct MessageMeta
    {
        uint16_t DestinationId;
        uint16_t Type;
        uint16_t Sequence;
        uint16_t Length;
        uint16_t Crc16;
    };
} // namespace SlaveSpi

#endif // MESSAGE_H