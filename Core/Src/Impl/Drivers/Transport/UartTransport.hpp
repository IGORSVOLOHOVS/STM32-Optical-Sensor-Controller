/**
 * @file UartTransport.hpp
 * @brief Interrupt-based UART transport layer for communication protocol
 * @author Igor S.
 * @date 2025-11-22
 * 
 * This file implements the UART transport layer using interrupt-driven reception
 * with std::atomic for thread-safe ISR communication. Uses USART1 (PC4/PC5).
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include <array>
#include <algorithm>
#include <ranges>
#include <atomic>
#include "main.h"
#include "../../Utils/Logger.hpp"

extern UART_HandleTypeDef huart1; ///< USART1 handle for communication

namespace Drivers {
namespace Transport {

/**
 * @class UartTransport
 * @brief Interrupt-based UART transport implementation
 * 
 * Provides non-blocking UART communication using HAL_UART_Receive_IT().
 * Uses atomic flag for thread-safe communication between ISR and main loop.
 * 
 * @note Uses USART1 (PC4/PC5) at 115200 baud
 * @note Packet size is fixed at 8 bytes
 * 
 * Example usage:
 * @code
 * UartTransport transport;
 * transport.init();
 * 
 * if (transport.available()) {
 *     uint8_t buffer[8];
 *     if (transport.receive(buffer, 8)) {
 *         // Process received data
 *     }
 * }
 * @endcode
 */
class UartTransport {
public:
    static constexpr size_t PACKET_SIZE = 8; ///< Fixed packet size in bytes
    
    /**
     * @brief Constructor - initializes buffers
     */
    UartTransport() : rxReady_(false) {
        std::ranges::fill(rxBuffer_, 0);
    }

    /**
     * @brief Initialize UART and start interrupt reception
     * @return true on success
     * @note Automatically starts receiving first packet
     */
    bool init() {
        Utils::Logger::log("UartTransport: Initialized (Interrupt mode)");
        // Start receiving first packet via interrupt
        HAL_UART_Receive_IT(&huart1, rxBuffer_.data(), PACKET_SIZE);
        return true;
    }

    /**
     * @brief Transmit data packet over UART
     * @param data Pointer to data buffer
     * @param length Number of bytes to transmit
     * @note Blocking call with 100ms timeout
     */
    void transmit(const uint8_t* data, size_t length) {
        Utils::Logger::log("UartTransport: Transmitting packet...");
        HAL_UART_Transmit(&huart1, (uint8_t*)data, (uint16_t)length, 100);
        
        // Log the transmitted data using ranges
        std::array<char, 64> logBuf;
        auto dataView = std::span{data, length};
        snprintf(logBuf.data(), logBuf.size(), "TX[%d]: %02X %02X %02X %02X %02X %02X %02X %02X", 
                 (int)length, dataView[0], dataView[1], dataView[2], dataView[3], 
                 dataView[4], dataView[5], dataView[6], dataView[7]);
        Utils::Logger::log(logBuf.data());
    }

    /**
     * @brief Check if data is available
     * @return true if complete packet received
     * @note Thread-safe using atomic load with acquire semantics
     */
    [[nodiscard]] bool available() {
        return rxReady_.load(std::memory_order_acquire);
    }

    /**
     * @brief Receive data packet from internal buffer
     * @param buffer Destination buffer
     * @param length Number of bytes to receive
     * @return true if data was available and copied
     * @note Automatically restarts interrupt reception after reading
     */
    bool receive(uint8_t* buffer, size_t length) {
        if (!rxReady_.load(std::memory_order_acquire)) {
            return false;
        }
        
        // Copy data using ranges
        auto sourceRange = rxBuffer_ | std::views::take(std::min(length, PACKET_SIZE));
        std::ranges::copy(sourceRange, buffer);
        
        // Log the received data
        std::array<char, 64> logBuf;
        snprintf(logBuf.data(), logBuf.size(), "RX[%d]: %02X %02X %02X %02X %02X %02X %02X %02X", 
                 (int)length, buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5], buffer[6], buffer[7]);
        Utils::Logger::log(logBuf.data());
        
        // Clear ready flag and restart reception
        rxReady_.store(false, std::memory_order_release);
        HAL_UART_Receive_IT(&huart1, rxBuffer_.data(), PACKET_SIZE);
        
        return true;
    }

    /**
     * @brief ISR callback - called when UART reception completes
     * @note Called from HAL_UART_RxCpltCallback() interrupt context
     * @warning Must be fast - only sets atomic flag
     */
    void onRxComplete() {
        rxReady_.store(true, std::memory_order_release);
        Utils::Logger::log("UartTransport: Packet received via interrupt");
    }

private:
    std::array<uint8_t, PACKET_SIZE> rxBuffer_; ///< DMA receive buffer
    std::atomic<bool> rxReady_;  ///< Thread-safe flag for ISR communication
};

/// Global instance pointer for interrupt callback access
extern UartTransport* g_uartTransport;

} // namespace Transport
} // namespace Drivers
