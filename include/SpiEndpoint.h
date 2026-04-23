#pragma once
#ifndef SPIENDPOINT_H
#define SPIENDPOINT_H
#include "Arduino.h"
#include "SlaveSpi.h"
#include <memory>
#include <optional>

namespace SlaveSpi
{
    class SpiEndpoint
    {
    private:
        /* data */
        const uint16_t endpointId;
    public:
        SpiEndpoint(uint16_t id); 
        ~SpiEndpoint();

        virtual std::optional<Response> onMessageReceived(const MessageMeta& meta, ArrayView<uint16_t> payload) = 0;
        const uint16_t getId() const { return endpointId; }
    };
    
    SpiEndpoint::SpiEndpoint(uint16_t id) : endpointId(id)
    {
    }
    
    SpiEndpoint::~SpiEndpoint()
    {
    }
    
} // namespace SlaveSpi

#endif