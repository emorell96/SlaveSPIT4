#if !defined(SLAVE_SPI_H)
#define SLAVE_SPI_H

#include "Arduino.h"
#include "Message.h"
#include "circular_buffer.h"
#include <functional>
#include <SPI.h>

namespace SlaveSpi
{
    enum class SpiSlaveParserState{
        Idle,
        
    };
    class SlaveSpiBase{
        public:
            virtual void SpiSlaveIsr() = 0;
    };

    struct SlaveRegisters
    {
        public:
            SlaveRegisters(volatile uint32_t* spiAddr) : spiAddr(spiAddr) {}
        #if defined(__IMXRT1062__)
            inline volatile uint32_t& CR() { return spiAddr[4]; } // CR is at offset 0x10, which is 4 32-bit words into the register map
            inline volatile uint32_t& FCR() { return spiAddr[22]; } 
            inline volatile uint32_t& FSR() { return spiAddr[23]; }
            inline volatile uint32_t& IER() { return spiAddr[6]; }
            inline volatile uint32_t& CFGR0() { return spiAddr[8]; }
            inline volatile uint32_t& CFGR1() { return spiAddr[9]; }
            inline void TDR(uint32_t value) { spiAddr[25] = value; }
            inline volatile uint32_t& RDR() { return spiAddr[29]; }
            inline volatile uint32_t& SR() { return spiAddr[5]; }
            inline volatile uint32_t& DMR0() { return spiAddr[12]; } // DMR0 is at offset 30h, which is 48 bytes -> 12 32-bit words into the register map, see p. 2811 ref manual rev2
            inline volatile uint32_t& DMR1() { return spiAddr[13]; } // DMR1 is at offset 34h, which is 52 bytes -> 13 32-bit words into the register map, see p. 2811 ref manual rev2
            inline void TCR_REFRESH() { spiAddr[24] = (2UL << 27) | LPSPI_TCR_FRAMESZ(16 - 1); }
            // inline void 
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
        SpiSlaveParserState parser_state = SpiSlaveParserState::Idle;
        std::function<void(const MessageMeta&, const uint32_t*)> message_received_callback;

        void clearRxBuffer(uint16_t messagesToClear);

        void readMessage(MessageMeta& meta, uint16_t* payloadBuffer);

        void _processIdleState();
    public:
        SlaveSpi(/* args */);
        ~SlaveSpi();
        
        void begin();
        void onMessageReceived(std::function<void(const MessageMeta, const uint32_t*)> callback);
        void SpiSlaveIsr() override;
        uint16_t processMessages();
    };
}

#endif // SLAVE_SPI_H