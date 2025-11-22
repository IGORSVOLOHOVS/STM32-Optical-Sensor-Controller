/**
 * @file main.cpp
 * @brief C++ entry point and system call implementations
 * @author Igor S.
 * @date 2025-11-22
 * 
 * Provides the C++ entry point (cpp_main) called from C main.c,
 * implements missing syscalls for C++20 compatibility, and handles
 * UART interrupt callbacks.
 */

#define USE_UART_TRANSPORT  ///< Select UART transport (comment out for CAN)

#include "App/Application.hpp"
#include <sys/types.h>

/**
 * @brief Stub implementation of entropy generation for C++ standard library
 * @param buffer Buffer to fill with random bytes (unused)
 * @param length Length of the buffer (unused)
 * @return 0 on success (always succeeds but provides no entropy)
 * 
 * @warning This is a STUB - does not provide real randomness!
 * @warning std::random_device will produce deterministic output
 * @note STM32C092RC has no hardware TRNG
 * @note For real security, implement using ADC noise or timer jitter
 */
extern "C" int _getentropy(void *buffer, size_t length) {
    (void)buffer;
    (void)length;
    
    // WARNING: This returns 0 (Success) but does absolutely nothing.
    // Your random numbers (std::random_device) will be deterministic (always the same)
    // unless you implement a software PRNG here.
    return 0; 
}

extern UART_HandleTypeDef huart2; ///< USART2 handle for VCP logging

/**
 * @brief Redirect stdout/stderr to UART for printf support
 * @param file File descriptor (ignored)
 * @param ptr Pointer to data to write
 * @param len Number of bytes to write
 * @return Number of bytes written
 * 
 * @note Enables printf() and std::cout to work over UART
 * @note Uses USART2 (VCP) with 100ms timeout
 */
extern "C" int _write(int file, char *ptr, int len) {
    (void)file;
    HAL_UART_Transmit(&huart2, (uint8_t*)ptr, len, 100);
    return len;
}

// Global pointer for interrupt callback
namespace Drivers { namespace Transport { extern UartTransport* g_uartTransport; } }
Drivers::Transport::UartTransport* Drivers::Transport::g_uartTransport = nullptr;

/**
 * @brief UART receive complete callback (ISR context)
 * @param huart UART handle that triggered the interrupt
 * 
 * @note Called from interrupt context - must be fast!
 * @note Only handles USART1 interrupts
 * @warning Do not call blocking functions here
 */
extern "C" void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART1 && Drivers::Transport::g_uartTransport != nullptr) {
        Drivers::Transport::g_uartTransport->onRxComplete();
    }
}

/**
 * @brief C++ entry point called from main.c
 * 
 * Creates static Application instance and runs the main loop.
 * This function never returns.
 * 
 * @note Called after all HAL initialization is complete
 * @note Uses static allocation to avoid heap usage
 */
extern "C" void cpp_main() {
    static App::Application<> app;
    app.run();
}
