# STM32 Optical Sensor Controller

Modern C++20 firmware for STM32C092RC microcontroller to manage 6 optical sensors with UART/CAN communication.

## 🎯 Features

- **6-Channel ADC**: DMA-based continuous sampling of optical sensors
- **Dual Communication**: UART and CAN protocols (compile-time selectable)
- **Modern C++20**: Template-heavy, header-only architecture with zero dynamic allocation
- **Interrupt-Driven**: Non-blocking UART reception via interrupts
- **Thread-Safe**: Atomic operations for ISR communication
- **Sensor Logic**: Hysteresis and debouncing for reliable state detection

## 📖 Documentation

- **[API Documentation](docs/html/index.html)**: Complete Doxygen-generated API reference
- **[Architecture](ARCHITECTURE.md)**: System diagrams and design overview
- **[README](README.md)**: This file - project overview

To regenerate documentation:
```bash
doxygen Doxyfile
```

## 🔧 Hardware

- **MCU**: STM32C092RC (ARM Cortex-M0+, 32-bit)
- **Board**: NUCLEO-C092RC
- **Sensors**: 6x Optical sensors (analog)
- **Communication**: 
  - USART1 (PC4/PC5) - Protocol communication
  - USART2 (PA2/PA3) - Debug logging via VCP
  - FDCAN1 - CAN bus (alternative to UART)

## 📋 Protocol

Zero-byte command protocol over UART/CAN (8-byte packets):

| Command | Description | R/W |
|---------|-------------|-----|
| 0x00 | ADC Values | Read |
| 0x01 | Transparent Liquid Sensitivity | R/W |
| 0x02 | Nontransparent Liquid Sensitivity | R/W |
| 0x03 | Time Delay | R/W |
| 0x04 | Sensor States | Read |
| 0x05 | Device ID | R/W |

**Packet Format**: `[CMD_RW][DATA0][DATA1]...[DATA6]`
- Bit 0 of CMD_RW: 0=Read, 1=Write
- Bits 1-7: Command ID

## 🏗️ Architecture

```
Core/Src/Impl/
├── App/
│   ├── Application.hpp      # Main application logic
│   └── Settings.hpp          # Configuration structure
├── Drivers/Transport/
│   ├── UartTransport.hpp     # UART driver (interrupt-based)
│   ├── CanTransport.hpp      # CAN driver (stub)
│   └── CommunicationManager.hpp  # Protocol handler
├── Hal/
│   ├── Adc.hpp               # ADC + DMA driver
│   └── AdcStub.hpp           # Testing stub
├── Utils/
│   └── Logger.hpp            # UART logging utility
└── main.cpp                  # C++ entry point
```

### Design Patterns

- **Policy-Based Design**: Compile-time transport selection (UART/CAN)
- **Template Metaprogramming**: Zero-overhead abstractions
- **RAII**: Resource management without exceptions
- **Modern C++20**: `std::array`, `std::ranges`, `std::atomic`, `std::span`

## 🚀 Building

### Prerequisites

- STM32CubeIDE 1.19.0+
- ARM GCC Toolchain (included with IDE)
- C++20 compiler support

### Compile-Time Configuration

Select transport in `Core/Src/Impl/main.cpp`:

```cpp
#define USE_UART_TRANSPORT  // Comment out for CAN
```

### Build Steps

1. Open project in STM32CubeIDE
2. Build: `Project → Build All` (Ctrl+B)
3. Flash: `Run → Debug` (F11)

## 🧪 Testing

### Arduino UART Tester

Located in `Tests/ArduinoTester/ArduinoTester.ino`

**Wiring**:
- Arduino Pin 11 (TX) → STM32 PC5 (USART1_RX)
- Arduino Pin 10 (RX) → STM32 PC4 (USART1_TX)
- GND → GND

**Usage**:
1. Upload sketch to Arduino Uno
2. Open Serial Monitor (115200 baud)
3. Observe command/response exchange

## 📊 Memory Usage

- **Flash**: ~15KB (with logging)
- **RAM**: ~2KB (static allocation only)
- **Stack**: ~1KB

## 🔒 Safety Features

- ✅ No dynamic allocation (`new`/`malloc` forbidden)
- ✅ No exceptions (`-fno-exceptions`)
- ✅ No RTTI (`-fno-rtti`)
- ✅ Atomic operations for ISR safety
- ✅ Compile-time polymorphism (zero virtual overhead)

## 📝 Code Style

- **C++20 Standard**: Modern features (ranges, concepts, constexpr)
- **Header-Only**: Template-heavy architecture
- **Const-Correctness**: Extensive use of `const` and `constexpr`
- **Type Safety**: Strong typing, no C-style casts
- **RAII**: Resource management

## 🐛 Debugging

Debug output via USART2 (VCP):
- Baud rate: 115200
- Format: 8N1
- Monitor: PuTTY, Tera Term, or Serial Monitor

Enable verbose logging in `UartTransport.hpp` to see all TX/RX packets.

## 📚 Documentation

- [STM32C0 Reference Manual](https://www.st.com/resource/en/reference_manual/rm0490-stm32c0x1-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)
- [HAL Driver Documentation](https://www.st.com/resource/en/user_manual/um2319-description-of-stm32c0-hal-and-lowlayer-drivers-stmicroelectronics.pdf)
- Protocol: `CAN_protocol (1).docx`

## 🤝 Contributing

1. Follow existing code style
2. Use modern C++20 features
3. No dynamic allocation
4. Add tests for new features
5. Update documentation

## 📄 License

Proprietary - All rights reserved

## 👤 Author

Igor S. - STM32 C++20 Transition Project

---

**Note**: This is a transition project from Arduino (AVR 8-bit) to STM32 (ARM 32-bit) with focus on modern C++ practices and embedded best practices.
