#include "SpiEndpoint.h"
#include <map>
#include <memory>

namespace SlaveSpi
{
    class SpiRouter
    {
    private:
        std::map<uint16_t, std::shared_ptr<SpiEndpoint>> endpoints; // map of endpoint ID to endpoint instance  
    public:
        SpiRouter(/* args */);
        ~SpiRouter();

        bool registerEndpoint(std::shared_ptr<SpiEndpoint> endpoint);
        void routeMessage(const MessageMeta& meta, ArrayView<uint16_t> payload);
    };
    
} // namespace SlaveSpi

inline SlaveSpi::SpiRouter::SpiRouter()
{
}

inline SlaveSpi::SpiRouter::~SpiRouter()
{
}

inline bool SlaveSpi::SpiRouter::registerEndpoint(std::shared_ptr<SpiEndpoint> endpoint)
{
    if(endpoints.find(endpoint->getId()) != endpoints.end())
    {
        // endpoint with this ID already registered
        return false;
    }
    endpoints[endpoint->getId()] = endpoint;
    return true;
}

void SlaveSpi::SpiRouter::routeMessage(const MessageMeta &meta, ArrayView<uint16_t> payload)
{
    if(endpoints.find(meta.Type) != endpoints.end())
    {
        endpoints[meta.Type]->onMessageReceived(meta, payload);
    }
    else
    {
        // no endpoint registered for this message type, ignore the message or handle it as needed
    }
}