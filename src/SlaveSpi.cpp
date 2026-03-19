#include "SlaveSpi.h"

template <SPIClass *SPIPort, uint16_t SlaveId, uint16_t BufferSize>
void SlaveSpi::SlaveSpi<SPIPort, SlaveId, BufferSize>::clearRxBuffer(uint16_t messagesToClear)
{
    for(uint16_t i = 0; i < messagesToClear; i++)
    {
        rx_message_buffer.pop_front();
    }
}

template <SPIClass *SPIPort, uint16_t SlaveId, uint16_t BufferSize>
void SlaveSpi::SlaveSpi<SPIPort, SlaveId, BufferSize>::readMessage(MessageMeta &meta, uint16_t *payloadBuffer)
{
    // read the payload only if the buffer is provided. If not read only the meta data, and discard the payload
    current_message_meta.DestinationId = rx_message_buffer.pop_front();
    current_message_meta.Type = rx_message_buffer.pop_front();
    current_message_meta.Sequence = rx_message_buffer.pop_front();
    current_message_meta.Length = rx_message_buffer.pop_front();

    if(payloadBuffer != nullptr)
    {
        for(uint16_t i = 0; i < current_message_meta.Length; i++)
        {
            payloadBuffer[i] = rx_message_buffer.pop_front();
        }
    }
    else
    {
        clearRxBuffer(current_message_meta.Length);
    }
}

template <SPIClass *SPIPort, uint16_t SlaveId, uint16_t BufferSize>
inline SlaveSpi::SlaveSpi<SPIPort, SlaveId, BufferSize>::SlaveSpi()
{
    if constexpr (SPIPort == &SPI) 
    {
        registers = SlaveRegisters((volatile uint32_t*)SPIPort->hardware_addr);
        nvic_irq = IRQ_LPSPI4;
        _LPSPI4 = this;
    }
    else if constexpr (SPIPort == &SPI1) 
    {
        registers = SlaveRegisters((volatile uint32_t*)SPIPort->hardware_addr);
        nvic_irq = IRQ_LPSPI3;
        _LPSPI3 = this;
    }
}

template <SPIClass *SPIPort, uint16_t SlaveId, uint16_t BufferSize>
uint16_t SlaveSpi::SlaveSpi<SPIPort, SlaveId, BufferSize>::processMessages()
{
    uint16_t word = rx_message_buffer.pop_front();
    // start of message
    if(word == 0xDEAD)
    {
        current_message_meta.DestinationId = rx_message_buffer.pop_front();
        if(current_message_meta.DestinationId != SlaveId)
        {
            // not for us, ignore message
            // but first clear the rest of the message from the buffer
            uint16_t messageLength = rx_message_buffer.pop_front();
            return 0;
        }

    }

    return 0;
}
