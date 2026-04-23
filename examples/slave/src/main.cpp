#include <SPI.h>
#include <SlaveSpi.h>
#include <SpiRouter.h>
#include <memory>
#include <SpiEndpoint.h>

std::shared_ptr<SlaveSpi::SlaveSpi<&SPI1, 0x1234>> spiSlave = std::make_shared<SlaveSpi::SlaveSpi<&SPI1, 0x1234>>();
SlaveSpi::SpiRouter spiRouter(spiSlave);

class BasicEndpoint : public SlaveSpi::SpiEndpoint
{
    using SlaveSpi::SpiEndpoint::SpiEndpoint;
    virtual std::optional<SlaveSpi::Response> onMessageReceived(const SlaveSpi::MessageMeta& meta, SlaveSpi::ArrayView<uint16_t> payload) override; 
};

std::optional<SlaveSpi::Response> BasicEndpoint::onMessageReceived(const SlaveSpi::MessageMeta& meta, SlaveSpi::ArrayView<uint16_t> payload)
{
    Serial.print("Received message with type: ");
    Serial.println(meta.Type);
    Serial.print("Payload length: ");
    Serial.println(meta.Length);
    for(size_t i = 0; i < payload.size(); i++){
        Serial.print("Payload[");
        Serial.print(i);
        Serial.print("]: ");
        Serial.println(payload[i]);
    }
    return std::nullopt;
}

void setup(){
    Serial.begin(115200);
    spiRouter.registerEndpoint(std::make_shared<BasicEndpoint>(0x0001));
    spiRouter.begin();
}

void loop(){
    spiSlave->processMessages();
}
