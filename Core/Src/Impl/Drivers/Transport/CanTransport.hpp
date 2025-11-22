#pragma once

#include <cstdint>
#include <cstddef>

namespace Drivers {
namespace Transport {

class CanTransport {
public:
    bool init() {
        // Stub: Initialize CAN hardware
        return true;
    }

    void transmit(const uint8_t* data, size_t length) {
        // Stub: Send data via CAN
        (void)data;
        (void)length;
    }

    bool available() {
        // Stub: Check if CAN message received
        return false;
    }

    bool receive(uint8_t* buffer, size_t length) {
        // Stub: Read CAN message
        (void)buffer;
        (void)length;
        return false;
    }
};

} // namespace Transport
} // namespace Drivers
