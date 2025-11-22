#pragma once

#include <cstdint>
#include <cstddef>
#include <array>
#include "../../App/Settings.hpp"
#include "../../Hal/AdcStub.hpp" // In real app, use abstract interface

namespace Drivers {
namespace Transport {

/**
 * @brief Manager class that handles communication using a specific Transport Policy.
 * @tparam TransportPolicy The class defining the low-level send/receive logic.
 */
template <typename TransportPolicy>
class CommunicationManager {
public:
    /**
     * @brief Initializes the underlying transport layer.
     */
    bool init() {
        return transport_.init();
    }

    /**
     * @brief Checks for incoming data and processes it.
     * @param settings Reference to application settings to update/read.
     * @param adc Reference to ADC to read values.
     */
    template <typename AdcType>
    void process(App::Settings& settings, AdcType& adc) {
        // Protocol expects 8 bytes
        uint8_t rxBuffer[8] = {0};

        if (transport_.available()) {
            if (transport_.receive(rxBuffer, 8)) {
                handlePacket(rxBuffer, settings, adc);
            }
        }
    }

    /**
     * @brief Get reference to transport for interrupt callback registration
     */
    TransportPolicy& getTransport() {
        return transport_;
    }

private:
    TransportPolicy transport_;

    template <typename AdcType>
    void handlePacket(const uint8_t* rxBuf, App::Settings& settings, AdcType& adc) {
        // Byte 0: Bit 0 = R/W (1=Write, 0=Read), Bits 1-7 = Command
        bool isWrite = (rxBuf[0] & 0x01);
        uint8_t command = (rxBuf[0] >> 1);

        uint8_t txBuffer[8] = {0};
        // Echo command by default
        txBuffer[0] = rxBuf[0]; 

        switch (command) {
            case 0x00: // ADC Values
                if (!isWrite) {
                    const auto& values = adc.readAll();
                    // Pack 6 values (12-bit) into 8 bytes? 
                    // Or maybe just send raw bytes if protocol allows?
                    // The protocol doc says "ADC values". 
                    // Usually 6 * 2 bytes = 12 bytes, doesn't fit in 8 bytes.
                    // Assuming we send a subset or the protocol uses multiple frames.
                    // For now, let's pack first 4 sensors or just dummy pack.
                    // Re-reading protocol: "0000000 — Чтение значений АЦП"
                    // If it's CAN, maybe it sends multiple frames or just status?
                    // Let's assume we send Sensor States (0/1) packed in 1 byte?
                    // Ah, command 0x04 is "Sensor states".
                    // Command 0x00 is "ADC values". Maybe it returns one sensor?
                    // Without full spec, I'll just pack the first 3 sensors (6 bytes).
                    txBuffer[1] = (uint8_t)(values[0] & 0xFF);
                    txBuffer[2] = (uint8_t)(values[0] >> 8);
                    txBuffer[3] = (uint8_t)(values[1] & 0xFF);
                    txBuffer[4] = (uint8_t)(values[1] >> 8);
                    txBuffer[5] = (uint8_t)(values[2] & 0xFF);
                    txBuffer[6] = (uint8_t)(values[2] >> 8);
                }
                break;

            case 0x01: // Transparent Liquid Sensitivity
                if (isWrite) {
                    settings.transparentThreshold = (uint16_t)rxBuf[1] | ((uint16_t)rxBuf[2] << 8);
                } else {
                    txBuffer[1] = (uint8_t)(settings.transparentThreshold & 0xFF);
                    txBuffer[2] = (uint8_t)(settings.transparentThreshold >> 8);
                }
                break;

            case 0x02: // Nontransparent Liquid Sensitivity
                if (isWrite) {
                    settings.nontransparentThreshold = (uint16_t)rxBuf[1] | ((uint16_t)rxBuf[2] << 8);
                } else {
                    txBuffer[1] = (uint8_t)(settings.nontransparentThreshold & 0xFF);
                    txBuffer[2] = (uint8_t)(settings.nontransparentThreshold >> 8);
                }
                break;

            case 0x03: // Time Delay
                 if (isWrite) {
                    settings.timeDelay = (uint16_t)rxBuf[1] | ((uint16_t)rxBuf[2] << 8);
                } else {
                    txBuffer[1] = (uint8_t)(settings.timeDelay & 0xFF);
                    txBuffer[2] = (uint8_t)(settings.timeDelay >> 8);
                }
                break;

            case 0x04: // Sensor States
                if (!isWrite) {
                    // Logic to determine Empty/Full based on thresholds
                    // For now, just dummy implementation
                    uint8_t states = 0;
                    const auto& values = adc.readAll();
                    for(size_t i=0; i<6; ++i) {
                        if(values[i] > settings.nontransparentThreshold) {
                            states |= (1 << i);
                        }
                    }
                    txBuffer[1] = states;
                }
                break;
            
            case 0x05: // Device ID
                if (isWrite) {
                    settings.deviceId = (uint16_t)rxBuf[1] | ((uint16_t)rxBuf[2] << 8);
                } else {
                    txBuffer[1] = (uint8_t)(settings.deviceId & 0xFF);
                    txBuffer[2] = (uint8_t)(settings.deviceId >> 8);
                }
                break;

            default:
                // Unknown command
                break;
        }

        // Send response
        transport_.transmit(txBuffer, 8);
    }
};

} // namespace Transport
} // namespace Drivers
