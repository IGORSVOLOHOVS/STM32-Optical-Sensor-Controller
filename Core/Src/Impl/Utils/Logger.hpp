/**
 * @file Logger.hpp
 * @brief UART-based logging utility for debugging
 * @author Igor S.
 * @date 2025-11-22
 * 
 * Provides logging functionality via USART2 (VCP) using modern C++20 features.
 * Supports string_view for efficient string handling without copying.
 */

#pragma once

#include "main.h"
#include <array>
#include <algorithm>
#include <cstdio>
#include <string_view>

extern UART_HandleTypeDef huart2; ///< USART2 handle for VCP logging

namespace Utils {

/**
 * @class Logger
 * @brief Static logging utility class
 * 
 * Provides thread-safe logging to USART2 (Virtual COM Port) for debugging.
 * All methods are static - no instantiation required.
 * 
 * Features:
 * - Uses std::string_view for zero-copy string handling
 * - Automatic newline appending
 * - Integer value logging with prefix
 * - Compile-time string optimization
 * 
 * Example usage:
 * @code
 * Logger::log("System initialized");
 * Logger::log("ADC value", 1234);
 * Logger::log(std::string_view("Dynamic string"));
 * @endcode
 * 
 * @note Uses USART2 at 115200 baud (8N1)
 * @note Maximum log message size is 128 bytes
 */
class Logger {
public:
    static constexpr size_t MAX_LOG_SIZE = 128; ///< Maximum log message length
    
    /**
     * @brief Log a string_view message
     * @param message Message to log (no null terminator required)
     * @note Automatically appends \r\n
     * @note Blocking call with 100ms timeout
     */
    static void log(std::string_view message) {
        // Transmit message
        HAL_UART_Transmit(&huart2, 
                         reinterpret_cast<const uint8_t*>(message.data()), 
                         static_cast<uint16_t>(message.size()), 
                         100);
        
        // Transmit newline
        static constexpr std::array<uint8_t, 2> newline = {'\r', '\n'};
        HAL_UART_Transmit(&huart2, newline.data(), newline.size(), 100);
    }
    
    /**
     * @brief Log a C-string message
     * @param message Null-terminated string
     * @note Convenience wrapper around string_view version
     */
    static void log(const char* message) {
        log(std::string_view(message));
    }

    /**
     * @brief Log a message with an integer value
     * @param prefix Message prefix
     * @param value Integer value to log
     * 
     * Example output: "ADC value: 1234\r\n"
     */
    static void log(std::string_view prefix, int value) {
        std::array<char, MAX_LOG_SIZE> buffer;
        int written = snprintf(buffer.data(), buffer.size(), "%.*s: %d", 
                              static_cast<int>(prefix.size()), 
                              prefix.data(), 
                              value);
        
        if (written > 0 && static_cast<size_t>(written) < buffer.size()) {
            log(std::string_view(buffer.data(), written));
        }
    }
    
    /**
     * @brief Log a message with an integer value (C-string overload)
     * @param prefix Null-terminated prefix string
     * @param value Integer value to log
     */
    static void log(const char* prefix, int value) {
        log(std::string_view(prefix), value);
    }
};

} // namespace Utils
