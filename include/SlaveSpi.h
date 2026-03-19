#if !defined(SLAVE_SPI_H)
#define SLAVE_SPI_H

#include "Arduino.h"
#include "Message.h"
#include "circular_buffer.h"
#include <functional>
#include <SPI.h>

namespace SlaveSpi
{
    class SlaveSpiBase{
        public:
            virtual void SpiSlaveIsr() = 0;
    };

    struct SlaveRegisters
    {
        public:
            SlaveRegisters(volatile uint32_t* spiAddr) : spiAddr(spiAddr) {}
        #if defined(__IMXRT1062__)
            inline volatile uint32_t& CR() { return spiAddr[4]; }
            inline volatile uint32_t& FCR() { return spiAddr[22]; }
            inline volatile uint32_t& FSR() { return spiAddr[23]; }
            inline volatile uint32_t& IER() { return spiAddr[6]; }
            inline volatile uint32_t& CFGR0() { return spiAddr[8]; }
            inline volatile uint32_t& CFGR1() { return spiAddr[9]; }
            inline void TDR(uint32_t value) { spiAddr[25] = value; }
            inline volatile uint32_t& RDR() { return spiAddr[29]; }
            inline volatile uint32_t& SR() { return spiAddr[5]; }
            inline void TCR_REFRESH() { spiAddr[24] = (2UL << 27) | LPSPI_TCR_FRAMESZ(16 - 1); }
        #endif
        private:
            volatile uint32_t* spiAddr = nullptr;
    };

    static SlaveSpiBase* _LPSPI4 = nullptr;
    static SlaveSpiBase* _LPSPI3 = nullptr;

    extern void __attribute__((weak)) lpspi4_slave_isr() {
        _LPSPI4->SpiSlaveIsr();
    }

    extern void __attribute__((weak)) lpspi3_slave_isr() {
        _LPSPI3->SpiSlaveIsr();
    }

    template <SPIClass* SPIPort, uint16_t SlaveId, uint16_t BufferSize = 128>  
    class SlaveSpi : public SlaveSpiBase
    {
    private:
        /* data */
        SlaveRegisters registers;
        uint32_t nvic_irq = 0;
        Circular_Buffer<uint16_t, BufferSize> rx_message_buffer;
        Circular_Buffer<uint16_t, BufferSize> tx_message_buffer;

        MessageMeta current_message_meta;
        std::function<void(const MessageMeta&, const uint32_t*)> message_received_callback;

        void clearRxBuffer(uint16_t messagesToClear);

        void readMessage(MessageMeta& meta, uint16_t* payloadBuffer);
    public:
        SlaveSpi(/* args */);
        ~SlaveSpi();
        
        void begin();
        void onMessageReceived(std::function<void(const MessageMeta&, const uint32_t*)> callback);
        void SpiSlaveIsr() override;
        uint16_t processMessages();
    };
}

#endif // SLAVE_SPI_H