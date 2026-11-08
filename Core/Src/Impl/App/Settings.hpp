/**
 * @file Settings.hpp
 * @brief Application configuration and settings structure
 * @author Igor S.
 * @date 2025-11-22
 *
 * Defines the Settings structure for storing sensor thresholds,
 * timing parameters, and device identification.
 */

#ifndef SOSC_APP_SETTINGS_HPP
#define SOSC_APP_SETTINGS_HPP

#include <cstdint>

namespace App {

/**
 * @struct Settings
 * @brief Configuration parameters for optical sensor system
 *
 * Contains all configurable parameters including sensor thresholds,
 * debouncing delays, and device identification. All values can be
 * modified at runtime via the communication protocol.
 *
 * Threshold Logic:
 * - emptyValue < transparentThreshold < nontransparentThreshold
 * - EMPTY: ADC < transparentThreshold
 * - FULL: ADC > nontransparentThreshold
 * - EMPTY_TEMPORARY: Between thresholds with timeDelay
 *
 * @note All members have sensible defaults
 * @note Supports constexpr initialization
 */
struct Settings {
    // Default values are constexpr-compatible
    uint16_t transparentThreshold = 1000;     ///< Threshold for transparent liquid detection
    uint16_t nontransparentThreshold = 3000;  ///< Threshold for non-transparent liquid detection
    uint16_t timeDelay = 100;                 ///< Debounce delay in milliseconds
    uint16_t deviceId = 0x123;                ///< Unique device identifier
    uint16_t filterId = 0x000;                ///< CAN filter ID (if using CAN)
    uint16_t emptyValue = 500;                ///< Baseline empty sensor value

    /**
     * @brief Constexpr constructor for compile-time initialization
     * @note Allows Settings to be used in constexpr contexts
     */
    constexpr Settings() noexcept = default;

    /**
     * @brief Validate settings for logical consistency
     * @return true if settings are valid
     * @retval false if thresholds are inconsistent
     *
     * Checks:
     * - transparentThreshold < nontransparentThreshold
     * - timeDelay > 0
     * - emptyValue < transparentThreshold
     */
    [[nodiscard]] constexpr bool isValid() const noexcept {
        return transparentThreshold < nontransparentThreshold && timeDelay > 0 && emptyValue < transparentThreshold;
    }
};

/**
 * @brief Constexpr default settings instance
 * @note Can be used for initialization or reset to defaults
 */
inline constexpr Settings DefaultSettings{};

}  // namespace App

#endif  // SOSC_APP_SETTINGS_HPP
