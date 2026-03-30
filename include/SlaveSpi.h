#if !defined(SLAVE_SPI_H)
#define SLAVE_SPI_H

#include <span>
#include "Arduino.h"
#include "Message.h"
#include "circular_buffer.h"
#include <functional>
#include <SPI.h>
#include "ArrayView.h"
#include "Crc16.h"

namespace SlaveSpi
{
    enum class SpiSlaveParserState{
        Idle,
        ClearBuffer,
        ReadMessage,
        CheckCrc
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
        if(_LPSPI4 == nullptr) return;
        _LPSPI4->SpiSlaveIsr();
    }

    extern void __attribute__((weak)) lpspi3_slave_isr() {
        if(_LPSPI3 == nullptr) return;
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
        std::vector<uint16_t> current_message_payload;

        SpiSlaveParserState parser_state = SpiSlaveParserState::Idle;
        std::function<void(const MessageMeta&, ArrayView<const uint16_t>)> message_received_callback;

        void clearRxBuffer(uint16_t messagesToClear);

        void readMessage(MessageMeta& meta, uint16_t* payloadBuffer);

        void _processIdleState();
        void _clearBuffer();
        void _readMessage();
        void _checkCrc();
    public:
        SlaveSpi(/* args */);
        ~SlaveSpi();
        
        void begin();
        void onMessageReceived(std::function<void(const MessageMeta&, ArrayView<const uint16_t>)> callback);
        void SpiSlaveIsr() override;
        uint16_t processMessages();
    };

    
}

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
    // current_message_meta.DestinationId = rx_message_buffer.pop_front();
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
void SlaveSpi::SlaveSpi<SPIPort, SlaveId, BufferSize>::_processIdleState()
{
    if(rx_message_buffer.available() < 5) // 5 is enough to fully read the metadata of the message
    {
        // not enough data for a message, wait for more data to arrive
        return;
    }
    uint16_t word = rx_message_buffer.pop_front();
    // start of message
    if(word == 0xDEAD) // if we enter this if statement, we know we have at least 5 words in the buffer, so we can safely read the next 4 words for the metadata without checking if they are available
    {
        // a message is structured as follows:
        // - 0xDEAD (16 bits) - start of message
        // - DEST_ID (16 bits) - destination ID of the message, only process messages with a matching ID // this part of the message will be matched by the SPI interface directly.
        // - TYPE (16 bits) - type of the message, used to determine how to process the message
        // - SEQUENCE (16 bits) - sequence number of the message, used to detect lost messages or out of order messages
        // - LENGTH (16 bits) - length of the payload in bytes
        // - PAYLOAD (variable length) - the actual data of the message, length is determined by the LENGTH field
        // - CRC (16 bits) - CRC of the message, used to detect corrupted messages
        current_message_meta.DestinationId = rx_message_buffer.pop_front();
        current_message_meta.Type = rx_message_buffer.pop_front();
        current_message_meta.Sequence = rx_message_buffer.pop_front();
        current_message_meta.Length = rx_message_buffer.pop_front(); // read the length of the payload, we need this to know how many words to clear from the buffer even if the message is not for us


        if(current_message_meta.DestinationId != SlaveId)
        {
            // not for us, ignore message
            // but first clear the rest of the message from the buffer
            // clear the rest of the message from the buffer
            this->parser_state = SpiSlaveParserState::ClearBuffer;
            return;
        }
        else
        {
            // clear the payload vector
            this->current_message_payload.clear();
            this->parser_state = SpiSlaveParserState::ReadMessage;
            return;
        }
    }
}

template <SPIClass *SPIPort, uint16_t SlaveId, uint16_t BufferSize>
void SlaveSpi::SlaveSpi<SPIPort, SlaveId, BufferSize>::_clearBuffer()
{
    while(rx_message_buffer.available() > 0 && current_message_meta.Length > -1)
    {
        // clear the data in the buffer until we reach the length of the message, and clear the CRC as well. (that's why we clear until the message length is -1)
        rx_message_buffer.pop_front();
        current_message_meta.Length--; // decrement the length of the message we are trying to clear, once this reaches 0, we know we have cleared the whole message from the buffer and can go back to idle state
    } // while there is still data in the buffer, keep clearing until we have cleared the whole message (including the CRC)
    
    if (current_message_meta.Length <= -1) // we have cleared the whole message plus the CRC from the buffer, we can go back to idle state
    {
        this->parser_state = SpiSlaveParserState::Idle;
    }
}

template <SPIClass *SPIPort, uint16_t SlaveId, uint16_t BufferSize>
void SlaveSpi::SlaveSpi<SPIPort, SlaveId, BufferSize>::_readMessage()
{
    while(rx_message_buffer.available() > 0 && current_message_meta.Length > 0)
    {
        // read the data from the buffer until we have read the whole payload of the message, once we have read the whole payload, we can call the callback function to process the message
        current_message_payload.push_back(rx_message_buffer.pop_front());
        current_message_meta.Length--; // decrement the length of the message we are trying to read, once this reaches 0, we know we have read the whole message from the buffer and can now check the CRC and call the callback function to process the message
    }
    if(current_message_meta.Length == 0)
    {
        // we have read the whole payload of the message, now we need to check the CRC and if it's correct, call the callback function to process the message
        this->parser_state = SpiSlaveParserState::CheckCrc;
    }
}

template <SPIClass *SPIPort, uint16_t SlaveId, uint16_t BufferSize>
void SlaveSpi::SlaveSpi<SPIPort, SlaveId, BufferSize>::_checkCrc()
{
    if(rx_message_buffer.available() == 0)
    {
        // we need to wait for the CRC to arrive, so just return and wait for the next interrupt to process the CRC
        return;
    }
    uint16_t received_crc = rx_message_buffer.pop_front();
    uint16_t calculated_crc = crc16_words(current_message_payload.data(), current_message_payload.size());

    if(received_crc == calculated_crc)
    {
        // CRC is correct, call the callback function to process the message
        if(message_received_callback)
        {
            message_received_callback(current_message_meta, ArrayView<const uint16_t>(current_message_payload));
        }
    }
    else
    {
        // CRC is incorrect, message is corrupted, ignore message
    }
    // after processing the message, go back to idle state to wait for the next message
    this->parser_state = SpiSlaveParserState::Idle;
}

template <SPIClass *SPIPort, uint16_t SlaveId, uint16_t BufferSize>
inline SlaveSpi::SlaveSpi<SPIPort, SlaveId, BufferSize>::SlaveSpi()
{

    if constexpr (SPIPort == &SPI) 
    {
        registers = SlaveRegisters((volatile uint32_t*)IMXRT_LPSPI4_ADDRESS);
        nvic_irq = IRQ_LPSPI4;
        _LPSPI4 = this;

        CCM_CCGR1 |= CCM_CCGR1_LPSPI4(1UL);

        attachInterruptVector(IRQ_NUMBER_t::IRQ_LPSPI4, lpspi4_slave_isr); // sets the interrupt in the NVIC table. The first 16 are reserved. After that the 32 is LPSPI1, 33 LPSPI2, 34 LPSPI3, 35 LPSPI4, etc. This is from the iMXRT1060 reference manual. p. 45

        IOMUXC_LPSPI4_SCK_SELECT_INPUT = 0;
        IOMUXC_LPSPI4_SDI_SELECT_INPUT = 0;
        IOMUXC_LPSPI4_SDO_SELECT_INPUT = 0;
        IOMUXC_LPSPI4_PCS0_SELECT_INPUT = 0;
    
        
        IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_03 = 0x3; /* LPSPI4 SCK (CLK) */ // SW_MUX_CTL_PAD_GPIO_B0_03 SW MUX Control -> p. 408 selects the mux mode to enable this GPIO B0 03 to be connected to the SCLK of LPSPI4
        IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_01 = 0x3; /* LPSPI4 SDI (MISO) */
        IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_02 = 0x3; /* LPSPI4 SDO (MOSI) */
        IOMUXC_SW_MUX_CTL_PAD_GPIO_B0_00 = 0x3; /* LPSPI4 PCS0 (CS) */
    }
    else if constexpr (SPIPort == &SPI1) 
    {
        registers = SlaveRegisters((volatile uint32_t*)IMXRT_LPSPI3_ADDRESS);
        nvic_irq = IRQ_LPSPI3;
        _LPSPI3 = this;

        CCM_CCGR1 |= CCM_CCGR1_LPSPI3(1UL); 

        attachInterruptVector(IRQ_NUMBER_t::IRQ_LPSPI3, lpspi3_slave_isr); // sets the interrupt in the NVIC table. The first 16 are reserved. After that the 32 is LPSPI1, 33 LPSPI2, 34 LPSPI3, 35 LPSPI4, etc. This is from the iMXRT1060 reference manual. p. 45

        IOMUXC_LPSPI3_SCK_SELECT_INPUT = 0x1; // sets the daisy setup. See p. 844. We use alt2 mode.
        IOMUXC_LPSPI3_SDI_SELECT_INPUT = 0x1; // sets the daisy setup. See p. 846. We use alt2 mode.
        IOMUXC_LPSPI3_SDO_SELECT_INPUT = 0x1; // sets the daisy setup. See p. 847. We use alt2 mode.
        IOMUXC_LPSPI3_PCS0_SELECT_INPUT = 0x1; // sets the daisy setup. See p. 845. We use alt2 mode.

        IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_15 = 0b010;// 0x2; /* LPSPI3 SCK (CLK) */ // SW_MUX_CTL_PAD_GPIO_AD_B1_15 SW MUX Control -> p. 507 selects the mux mode to enable this GPIO AD B1 15 to be connected to the SCLK of LPSPI3
        IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_14 = 0b010; /* LPSPI3 SDO (MOSI) */ // 010 // see p. 506 of reference manual https://www.pjrc.com/teensy/IMXRT1060RM_rev2.pdf
        IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_13 = 0b010; /*LPSPI3 SDI (MISO) */ // 010
        IOMUXC_SW_MUX_CTL_PAD_GPIO_AD_B1_12 = 0b010; /* LPSPI3 PCS0 (CS) */ // 010 // see p. 504 of reference manual https://www.pjrc.com/teensy/IMXRT1060RM_rev2.pdf
    }
}

template <SPIClass *SPIPort, uint16_t SlaveId, uint16_t BufferSize>
void SlaveSpi::SlaveSpi<SPIPort, SlaveId, BufferSize>::begin()
{
#if defined(__IMXRT1062__)
  registers.CR()  = LPSPI_CR_RST; /* Reset Module */
  registers.CR() = 0; /* Disable Module */
  registers.FCR() = 0;
  registers.IER() = LPSPI_IER_RDIE | LPSPI_IER_FCIE; /* Interrupt enable bits */ /* RX Interrupt */ /*Enable Frame End Interrupt too -> this is at the end of CS high (CS is active low) -> end of message -> reset data match*/
  registers.CFGR0() = 0;
  // for CFGR0 the following things should be configured:
  // RDMO (9) 

  registers.CFGR1() = 0; // LPSPI_CFGR1_OUTCFG;

  registers.DMR0() = (0xDEAD << 16) | SlaveId; /* Match word, upper 16 bits are the start of message, lower 16 bits are the destination ID. This is used by the hardware to automatically filter out messages that are not for this slave, and only trigger an interrupt when a message with a matching destination ID is received. */
  registers.CFGR0() |= LPSPI_CFGR0_RDMO; // enable data match, all data is discarded unless SR[DMF] = 1

  // for CFGR1 the following things should be configured:
  // PINCFG (25-24) = 00b -> SIN is input, SOUT is output
  // MATCFG (18-16) = 000b -> match disabled, 010b  match if the 1st data word matches DMR0.
  // PCSPOL (11 - 8) = 0000b -> active low (CS)
  // AUTOPCS (2) = 0b1 -> automatic negation of PCS.
  // Sample (1) is ignored in slave mode
  // Master/Slave (0) is 0 for slave mode
  registers.CFGR1() = LPSPI_CFGR1_MATCFG(0b010) | LPSPI_CFGR1_AUTOPCS;  //0b100;

  
  registers.SR() = 0; /* Clear status register */
  // SLAVE_TCR_REFRESH;
  registers.TDR(0x0); /* dummy data, must populate initial TX slot */
  // SLAVE_CR |= LPSPI_CR_MEN | LPSPI_CR_DBGEN | LPSPI_CR_DOZEN; /* Enable Module, Debug Mode, Doze Mode */

  // NVIC_ENABLE_IRQ(nvic_irq);
  // NVIC_SET_PRIORITY(nvic_irq, 1);
  registers.CR() = LPSPI_CR_MEN; /* Enable Module, Debug Mode */
  NVIC_ENABLE_IRQ(nvic_irq);
  NVIC_SET_PRIORITY(nvic_irq, 1);
#endif  
}

template <SPIClass *SPIPort, uint16_t SlaveId, uint16_t BufferSize>
void SlaveSpi::SlaveSpi<SPIPort, SlaveId, BufferSize>::SpiSlaveIsr()
{
    // this handles the ISR created by the SPI. Two options so far: data received, or frame complete (CS goes high). In the first case we read the data from the RDR and put it in the rx_message_buffer. In the second case we set a flag to indicate that a message is ready to be processed, and reset the parser state to idle.
    uint32_t status = registers.SR(); // takes a "snapshot" of the status by saving it into a variable
    if(status & LPSPI_SR_FCF && status & LPSPI_SR_DMF) // frame complete, CS goes high, end of message and a data match has happened. i.e. we are receiving a message that is for us, and the message is complete (CS goes high)
    {
        // clear RDMO
        registers.CFGR0() &= ~LPSPI_CFGR0_RDMO; // clear the RDMO bit to allow the next message to be received.
        // clear DMF flag
        registers.SR() &= ~LPSPI_SR_DMF;
        // clear FCF flag
        registers.SR() &= ~LPSPI_SR_FCF;
        //reenable data match for the next message
        registers.CFGR0() |= LPSPI_CFGR0_RDMO; // set the RDMO bit to allow the next message to be received.

        // set a flag to indicate that a message is ready to be processed
        // reset the parser state to idle
        parser_state = SpiSlaveParserState::Idle;
    }
    else if (status & LPSPI_SR_RDF) // data received, there is data in the RDR register that needs to be read
    {
        uint32_t data = registers.RDR(); // read the data from the RDR register, this also clears the RDF flag in the status register
        this->rx_message_buffer.push_back((data >> 16) & 0xFFFF); // divide the 32-bit data into two 16-bit words and push them into the rx_message_buffer
        this->rx_message_buffer.push_back(data & 0xFFFF);
    }

}

template <SPIClass *SPIPort, uint16_t SlaveId, uint16_t BufferSize>
uint16_t SlaveSpi::SlaveSpi<SPIPort, SlaveId, BufferSize>::processMessages()
{
    // a message is structured as follows:
    // - 0xDEAD (16 bits) - start of message
    // - DEST_ID (16 bits) - destination ID of the message, only process messages with a matching ID // this part of the message will be matched by the SPI interface directly.
    // - TYPE (16 bits) - type of the message, used to determine how to process the message
    // - SEQUENCE (16 bits) - sequence number of the message, used to detect lost messages or out of order messages
    // - LENGTH (16 bits) - length of the payload in bytes
    // - PAYLOAD (variable length) - the actual data of the message, length is determined by the LENGTH field
    // - CRC (16 bits) - CRC of the message, used to detect corrupted messages
    switch (parser_state)
    {
    case SpiSlaveParserState::Idle:
        this->_processIdleState(); // processes the idle state, i.e. when the parser is not reading anything yet.
        break;
    case SpiSlaveParserState::ClearBuffer:
        this->_clearBuffer(); // processes the clear buffer state, i.e. when the parser is trying to clear the rest of a message that is not for this slave from the buffer.
        break;
    case SpiSlaveParserState::ReadMessage:
        this->_readMessage(); // processes the read message state, i.e. when the parser has determined that a message is for this slave
        break;
    case SpiSlaveParserState::CheckCrc:
        this->_checkCrc(); // processes the check CRC state, i.e. when the parser has read a full message and now needs to check if the message is valid by checking the CRC, and if it is valid, call the callback function to process the message.
        break;
    default:
        break;
    }


    

    return 0;
}



#endif // SLAVE_SPI_H