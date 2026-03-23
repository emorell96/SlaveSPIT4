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
        return 0;
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
            this->clearRxBuffer(current_message_meta.Length + 2); // +2 for the Type and Sequence fields
            return 0;
        }
        onMessageReceived(current_message_meta, )
        

    }
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
    else(status & LPSPI_SR_RDF) // data received, there is data in the RDR register that needs to be read
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
    
    default:
        break;
    }


    

    return 0;
}
