/**
 * @file Adc.hpp
 * @brief DMA-based ADC driver for 6-channel optical sensor reading
 * @author Igor S.
 * @date 2025-11-22
 * 
 * Implements continuous ADC sampling using DMA in circular mode.
 * Reads 6 optical sensors simultaneously without CPU intervention.
 */

#pragma once

#include "main.h"
#include <array>
#include <cstddef>

extern ADC_HandleTypeDef hadc1; ///< ADC1 handle initialized in main.c

namespace Hal {

/**
 * @class Adc
 * @brief Hardware ADC driver with DMA support
 * 
 * Provides continuous 12-bit ADC sampling of 6 channels using DMA circular mode.
 * The DMA automatically updates the buffer without CPU intervention.
 * 
 * Features:
 * - Automatic calibration on init
 * - DMA circular buffer for continuous sampling
 * - Thread-safe reading (DMA writes, CPU reads)
 * - Zero CPU overhead during sampling
 * 
 * Hardware configuration (CubeMX):
 * - ADC1: 12-bit resolution, continuous mode
 * - DMA: Circular mode, half-word transfers
 * - Channels: 6 channels (configure rank order in CubeMX)
 * 
 * Example usage:
 * @code
 * Adc adc;
 * adc.init();
 * 
 * const auto& values = adc.readAll();
 * for (size_t i = 0; i < Adc::ChannelCount; ++i) {
 *     // Process values[i]
 * }
 * @endcode
 * 
 * @note Ensure CubeMX channel rank order matches array index
 * @warning DMA buffer is volatile - compiler won't optimize reads
 */
class Adc {
public:
    static constexpr size_t ChannelCount = 6; ///< Number of ADC channels

    /**
     * @brief Initialize ADC with calibration and start DMA
     * @note Performs ADC calibration before starting
     * @note Starts DMA in circular mode automatically
     */
    void init() noexcept {
        // Calibrate ADC before starting
        HAL_ADCEx_Calibration_Start(&hadc1);

        // Start ADC in DMA mode (Circular buffer)
        // Note: The buffer size is ChannelCount (6). 
        // Ensure the rank order in CubeMX matches the array index expectation.
        HAL_ADC_Start_DMA(&hadc1, (uint32_t*)dmaBuffer_, ChannelCount);
    }

    /**
     * @brief Read all ADC channels
     * @return Const reference to array of 6 ADC values (0-4095)
     * @note Values are copied from volatile DMA buffer to stable array
     * @note Thread-safe: DMA writes, this function reads
     */
    [[nodiscard]] const std::array<uint16_t, ChannelCount>& readAll() noexcept {
        // Since we are using DMA in circular mode, the buffer is always up to date.
        // We just return the current buffer state.
        
        for(size_t i=0; i<ChannelCount; ++i) {
            readings_[i] = dmaBuffer_[i];
        }
        return readings_;
    }

private:
    /**
     * @brief DMA destination buffer (hardware writes here)
     * @note volatile prevents compiler optimization
     * @note C-style array required (std::array doesn't support volatile well)
     */
    volatile uint16_t dmaBuffer_[ChannelCount];
    
    /**
     * @brief Stable shadow buffer for application use
     * @note Non-volatile copy for safe application access
     */
    std::array<uint16_t, ChannelCount> readings_{};
};

} // namespace Hal
