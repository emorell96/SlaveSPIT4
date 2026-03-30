#include "Arduino.h"
#include "SPI.h"
#include "Crc16.h"

namespace SlaveSpi
{
    /// @brief This class wraps the SPI library to provide an easy to use interface for the master to send messages to the slave. The master can send messages with a specific format, and the slave will parse the messages and call a callback function to process the messages. The message format is as follows:
    /// - 0xDEAD (16 bits) - start of message
    /// - DEST_ID (16 bits) - destination ID of the message, only process messages with a matching ID // this part of the message will be matched by the SPI interface directly.
    /// - TYPE (16 bits) - type of the message, used to determine how to
    /// - SEQUENCE (16 bits) - sequence number of the message, used to detect lost messages or out of order messages
    /// - LENGTH (16 bits) - length of the payload in bytes
    /// - PAYLOAD (variable length) - the actual data of the message, length is determined by the LENGTH field
    /// - CRC (16 bits) - CRC of the message, used to detect corrupted messages
    /// @tparam SPIPort 
    /// @tparam SlaveId 
    /// @tparam BufferSize 
    template <SPIClass *SPIPort, uint16_t SlaveId, uint16_t BufferSize>
    class MasterSpi
    {
        public:
            MasterSpi(uint8_t chipSelectPin, SPISettings settings = SPISettings(1000000, MSBFIRST, SPI_MODE0)) : spi_settings(settings) {
                SPIPort->begin();
                pinMode(chipSelectPin, OUTPUT);
                digitalWrite(chipSelectPin, HIGH); // set the chip select pin to high (inactive)
                this->chipSelectPin = chipSelectPin;
            }

            ~MasterSpi();
            uint8_t transfer16(uint16_t* data, size_t length, uint16_t type = 0, uint16_t sequence = 0);
            void setSettings(SPISettings settings) { spi_settings = settings; }
            
        private:
            SPISettings spi_settings = SPISettings(1000000, MSBFIRST, SPI_MODE0);
            uint8_t chipSelectPin;
    };

    template <SPIClass *SPIPort, uint16_t SlaveId, uint16_t BufferSize>
    inline uint8_t MasterSpi<SPIPort, SlaveId, BufferSize>::transfer16(uint16_t *data, size_t length, uint16_t type, uint16_t sequence)
    {
        uint16_t crc = crc16_words(data, length);
        SPIPort->beginTransaction(spi_settings);
        digitalWriteFast(chipSelectPin, LOW); // set the chip select pin to low (active)
        SPIPort->transfer16(0xDEAD); // start of message
        SPIPort->transfer16(SlaveId); // destination ID
        SPIPort->transfer16(type); // type
        SPIPort->transfer16(sequence); // sequence
        SPIPort->transfer16(length); // length
        for(size_t i = 0; i < length; i++)
        {
            SPIPort->transfer16(data[i]); // payload
        }
        SPIPort->transfer16(crc); // CRC
        digitalWriteFast(chipSelectPin, HIGH); // set the chip select pin to high (inactive)
        SPIPort->endTransaction();
        return 0;
    }
}