#include "Arduino.h"
#include "SPI.h"
#include "Crc16.h"
#include "Message.h"
#include <optional>

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
            std::optional<SlaveSpi::Response> transfer16(MessageMeta& meta, uint16_t* data, bool& succes, bool expecting_response = false);
            void setSettings(SPISettings settings) { spi_settings = settings; }
            void setTimeout(uint16_t timeout) { this->timeout = timeout; }
            
        private:
            SPISettings spi_settings = SPISettings(1000000, MSBFIRST, SPI_MODE0);
            uint8_t chipSelectPin;

            uint16_t timeout = 255; // timeout in # of bytes sent without a response. The master will wait for a response for this many bytes before giving up and returning a timeout error.
    };

    template <SPIClass *SPIPort, uint16_t SlaveId>
    inline std::optional<SlaveSpi::Response> MasterSpi<SPIPort, SlaveId>::transfer16(SlaveSpi::MessageMeta& meta, uint16_t* data, bool& success, bool expecting_response)
    {
        meta.Crc16 = crc16_words(data, meta.Length);
        SPIPort->beginTransaction(spi_settings);
        digitalWriteFast(chipSelectPin, LOW); // set the chip select pin to low (active)
        SPIPort->transfer16(0xDEAD); // start of message
        SPIPort->transfer16(meta.DestinationId ); // destination ID
        SPIPort->transfer16(meta.Type); // type
        SPIPort->transfer16(meta.Sequence); // sequence
        SPIPort->transfer16(meta.Length); // length
        for(size_t i = 0; i < meta.Length; i++)
        {
            SPIPort->transfer16(data[i]); // payload
        }
        SPIPort->transfer32(meta.Crc16 << 16); // CRC

        if(expecting_response)
        {
            // be ready to parse the response
            // a message is structured as follows:
            // - 0xDEAD (16 bits) - start of message
            // - DEST_ID (16 bits) - destination ID of the message, only process messages with a matching ID // this part of the message will be matched by the SPI interface directly.
            // - TYPE (16 bits) - type of the message, used to determine how to process the message
            // - SEQUENCE (16 bits) - sequence number of the message, used to detect lost messages or out of order messages
            // - LENGTH (16 bits) - length of the payload in bytes
            // - PAYLOAD (variable length) - the actual data of the message, length is determined by the LENGTH field
            // - CRC (16 bits) - CRC of the message, used to detect corrupted messages
            uint16_t response = SPIPort->transfer16(0x0000); // read the start of message
            uint16_t count = 0;
            while(response != 0xDEAD && count < this->timeout) // wait for the start of message
            {
                response = SPIPort->transfer16(0x0000);
                count++;
            }

            if(count >= this->timeout)
            {
                // timeout, no response received
                SPIPort->endTransaction();
                digitalWriteFast(chipSelectPin, HIGH); // set the chip select pin to high (inactive)
                success = false;
                return std::nullopt;
            }


            SlaveSpi::MessageMeta responseMeta;

            responseMeta.DestinationId = SPIPort->transfer16(0x00);
            responseMeta.Type = SPIPort->transfer16(0x00);
            responseMeta.Sequence = SPIPort->transfer16(0x00);
            responseMeta.Length = SPIPort->transfer16(0x00);
            #if defined(MASTER_SPI_DEBUG)
            // print the response meta for debug purposes
            Serial.print("Response meta - DestinationId: "); Serial.print(responseMeta.DestinationId, HEX);
            Serial.print(" Type: "); Serial.print(responseMeta.Type, HEX);
            Serial.print(" Sequence: "); Serial.print(responseMeta.Sequence, HEX);
            Serial.print(" Length: "); Serial.println(responseMeta.Length, HEX);
            #endif
            
            if(responseMeta.DestinationId == SlaveId && responseMeta.Sequence == meta.Sequence) // check if the response is for us
            {
                // response is for us, read it.
                SlaveSpi::Response responseMessage(responseMeta.Length);
                for(size_t i = 0; i < responseMeta.Length; i++)
                {
                    responseMessage.payload[i] = SPIPort->transfer16(0x0000); // read the payload
                }
                responseMessage.meta = responseMeta;
                uint16_t crc_received = SPIPort->transfer16(0x0000); // read the CRC
                uint16_t crc_calculated = crc16_words(responseMessage.payload.data(), responseMeta.Length); // calculate the CRC of the received message

                SPIPort->endTransaction();
                digitalWriteFast(chipSelectPin, HIGH); // set the chip select pin to high (inactive)

                if(crc_received == crc_calculated) // check if the CRC is correct
                {
                    success = true;
                    return responseMessage; // return the response message
                }
                else
                {
                    success = false;
                    return std::nullopt; // CRC is incorrect, return nullopt
                }
            }
        }


        // SPIPort->transfer16(crc);
        SPIPort->endTransaction();

        digitalWriteFast(chipSelectPin, HIGH); // set the chip select pin to high (inactive)
        #if defined(MASTER_SPI_DEBUG)
        Serial.print("Crc16: ");
        Serial.println(meta.Crc16, HEX);
        #endif
        success = true;
        return std::nullopt;
    }
}