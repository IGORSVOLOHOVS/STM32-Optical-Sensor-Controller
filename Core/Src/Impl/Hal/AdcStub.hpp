#pragma once

#include <cstdint>
#include <array>
#include <cstddef>

namespace Hal {

/**
 * @brief Stub class for ADC to simulate sensor readings.
 * Represents 6 optical sensors.
 */
class AdcStub {
public:
    static constexpr size_t ChannelCount = 6;

    /**
     * @brief Initializes the ADC stub.
     */
    constexpr void init() noexcept {
        // Simulation: Seed random or reset state if needed
    }

    /**
     * @brief Reads all channels via DMA (Simulated).
     * In a real implementation, this would trigger DMA or read buffer.
     * @return Reference to the array of latest readings.
     */
    [[nodiscard]] const std::array<uint16_t, ChannelCount>& readAll() noexcept {
        // Simulate some data
        // For now, return static dummy data or incrementing values
        // to verify the loop is working.
        static uint16_t counter = 0;
        counter++;
        
        for (size_t i = 0; i < ChannelCount; ++i) {
            // Simulate varying levels. 
            // If counter is odd, return high (FULL), else low (EMPTY)
            // Just a simple toggle for testing logic.
            readings_[i] = (counter % 20 > 10) ? 3000 : 500; 
        }
        return readings_;
    }

private:
    std::array<uint16_t, ChannelCount> readings_{};
};

} // namespace Hal
