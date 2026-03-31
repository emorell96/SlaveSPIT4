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
    template <SPIClass *SPIPort, uint16_t SlaveId>
    class MasterSpi
    {
        public:
            MasterSpi(uint8_t chipSelectPin = 254, SPISettings settings = SPISettings(1000000, MSBFIRST, SPI_MODE0)) : spi_settings(settings) 
            {
                if(chipSelectPin == 254)
                {
                    if(SPIPort == &SPI)
                    {
                        chipSelectPin = 10; // default chip select pin for SPI on the Teensy 4.1
                    }
                    else if (SPIPort == &SPI1)
                    {
                        chipSelectPin = 38; // default chip select pin for SPI1 on the Teensy 4.1
                    }
                }
                this->chipSelectPin = chipSelectPin;
            }
            void begin()
            {
                SPIPort->begin();
                pinMode(chipSelectPin, OUTPUT);
                digitalWrite(chipSelectPin, HIGH); // set the chip select pin to high (inactive)
            }

            ~MasterSpi() 
            {

            }
            uint8_t transfer16(uint16_t* data, size_t length, uint16_t type = 0, uint16_t sequence = 0);
            void setSettings(SPISettings settings) { spi_settings = settings; }
            
        private:
            SPISettings spi_settings = SPISettings(1000000, MSBFIRST, SPI_MODE0);
            uint8_t chipSelectPin;
    };

    template <SPIClass *SPIPort, uint16_t SlaveId>
    inline uint8_t MasterSpi<SPIPort, SlaveId>::transfer16(uint16_t *data, size_t length, uint16_t type, uint16_t sequence)
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
        SPIPort->transfer32(crc << 16); // CRC
        // SPIPort->transfer16(crc);
        SPIPort->endTransaction();

        digitalWriteFast(chipSelectPin, HIGH); // set the chip select pin to high (inactive)
        #if defined(MASTER_SPI_DEBUG)
        Serial.print("Crc16: ");
        Serial.println(crc, HEX);
        #endif
        return 0;
    }
}