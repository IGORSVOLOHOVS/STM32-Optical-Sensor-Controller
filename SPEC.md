# OpticManager - Software Requirements Specification (reverse-engineered)

This specification is reconstructed from the code of commit `5feba9c` (branch `dev`). It
describes what the firmware does at this commit, not what it is meant to do. Each requirement
names the file and line it was read from. Behaviour that is described in `README.md`,
`ARCHITECTURE.md` or code comments but has no code is listed in 3.11, not as a requirement.
Two findings change how the requirements read on the target. They are stated once here and
referenced as D-1 and D-2:

- **D-1 (UART path not wired).** `Drivers::Transport::g_uartTransport` is defined as `nullptr`
  (`Core/Src/Impl/main.cpp:58`) and nothing in `Core/` assigns it; `getTransport()`
  (`Core/Src/Impl/Drivers/Transport/CommunicationManager.hpp:46-48`) is never called. The RX
  callback therefore never reaches `UartTransport::onRxComplete()` (`main.cpp:69-71`), the ready
  flag stays false, and the receive interrupt is not re-armed after the first 8 bytes, because
  re-arming happens only in `UartTransport::receive()` (`UartTransport.hpp:125`). **Inference
  from code, not observed on hardware:** in the UART build the device does not answer protocol
  requests.
- **D-2 (ADC sampled once).** ADC1 has `ContinuousConvMode = DISABLE` (`Core/Src/main.c:187`)
  and `DMAContinuousRequests = DISABLE` (`main.c:192`). The DMA channel is in `DMA_NORMAL` mode
  (`Core/Src/stm32c0xx_hal_msp.c:138`, `OpticManager.ioc:15`), and `Adc::readAll()` never
  restarts a conversion (`Core/Src/Impl/Hal/Adc.hpp:78-86`). **Inference, not observed on
  hardware:** one scan sequence is converted after `Adc::init()`, and `readAll()` then returns
  the same six values on every call. This contradicts "continuous" in `Adc.hpp:7-8,25` and
  `README.md:7`.

## 1. Purpose and scope

The firmware runs on an STM32C092RC. It reads six analog optical-sensor inputs and exposes them,
plus four runtime settings, through a fixed 8-byte request/response protocol (`README.md:3`,
`README.md:37-50`).

**In scope (implemented):** peripheral initialisation; ADC1 + DMA acquisition of six channels;
the 8-byte protocol handler (commands 0x00-0x05); the interrupt-based UART transport on USART1;
debug logging on USART2; the compile-time transport switch; the `AdcStub` simulation class; the
Arduino UART tester sketch.

**Out of scope / absent at this commit:** a working CAN transport (stub only); sensor
debouncing, hysteresis and the EMPTY/FULL/EMPTY_TEMPORARY state machine; LED output;
non-volatile storage of settings; the user button; automated tests; CI.

## 2. Context

| Item | Value | Source |
|---|---|---|
| MCU | STM32C092RCTx (CPN STM32C092RCT6), LQFP64, Arm Cortex-M0+ | `OpticManager.ioc:39,52-53`, `README.md:27` |
| Board | NUCLEO-C092RC | `OpticManager.ioc:6,234` |
| Clock | HSI 48 MHz, SYSCLK = HCLK = PCLK = 48 MHz, flash latency 1 | `Core/Src/main.c:130-156`, `OpticManager.ioc:205-227` |
| Memory | FLASH 256 KB at 0x08000000, RAM 30 KB at 0x20000000 | `STM32C092RCTX_FLASH.ld:47-48` |
| Runtime | Bare metal, no RTOS; `Reset_Handler` -> `SystemInit` -> `__libc_init_array` -> `main` | `Core/Startup/startup_stm32c092rctx.s:51-91` |
| Language | C (CubeMX-generated layer) and C++20 (`Core/Src/Impl`) | `README.md:3`, `Doxyfile:263-264` |
| HAL | STM32C0xx HAL 1.4.0, CMSIS Core 5.6, vendored in `Drivers/` | `Drivers/STM32C0xx_HAL_Driver/Src/stm32c0xx_hal.c:58-61`, `Drivers/CMSIS/Include/cmsis_version.h:35-36` |

**Actors and external systems**

| Actor | Role | Evidence |
|---|---|---|
| Protocol master on USART1 (or CAN) | Sends 8-byte requests, receives 8-byte responses | `CommunicationManager.hpp:32-146`; a Raspberry Pi is named as master in `ARCHITECTURE.md:233,356` (no code for it in this repository) |
| Arduino Uno test tool | Sends a fixed command cycle over SoftwareSerial | `Tests/ArduinoTester/ArduinoTester.ino:1-92` |
| PC serial monitor on the ST-LINK VCP (USART2) | Receives the debug log | `Core/Src/Impl/Utils/Logger.hpp:19,56-66`, `README.md:140-143` |
| Six analog optical sensors | Drive ADC1 inputs | `README.md:29`, `Core/Src/stm32c0xx_hal_msp.c:111-127` |

## 3. Functional requirements

Verification: the repository has no automated tests. Where a requirement says "host check", it was reproduced
on 2026-11-18 by an off-repository harness. The harness compiled `CommunicationManager.hpp`,
`Settings.hpp` and `AdcStub.hpp` with clang++ 20 on the PC, using a mock transport (see 8.3). It is not a project test.

### 3.1 Start-up

- **FR-1.** The system shall call `HAL_Init`, `SystemClock_Config`, then initialise GPIO, DMA, ADC1, FDCAN1, USART1, USART2 in that order, and then call `cpp_main()`. Source: `Core/Src/main.c:86-107`. Verification: no test.
- **FR-2.** The system shall enter `Error_Handler()` if any CubeMX init call returns other than `HAL_OK`. `Error_Handler()` disables interrupts and loops forever. Source: `Core/Src/main.c:139-142,197-200,469-478`. Verification: no test.
- **FR-3.** `cpp_main()` shall construct one function-local static `App::Application<>` and call `run()`, which does not return. Source: `Core/Src/Impl/main.cpp:83-86`, `Core/Src/Impl/App/Application.hpp:29-52`. Verification: no test.
- **FR-4.** `Application::init()` shall log `Initializing Application...`, initialise the transport, initialise the ADC, and log `Initialization Complete.`. Source: `Application.hpp:22-27`. Verification: no test.
- **FR-5.** At GPIO init the system shall drive LED_GREEN (PA5) and LED_BLUE (PC9) high, drive CAN_STBY (PD2) low, and configure USER_BTN (PC13) as an input with pull-up. Source: `Core/Src/main.c:421-454`, `Core/Inc/main.h:60-83`. Verification: no test.

### 3.2 Transport selection

- **FR-6.** The system shall use `UartTransport` when `USE_UART_TRANSPORT` is defined and `CanTransport` otherwise. The macro is defined in `main.cpp`. Source: `Core/Src/Impl/main.cpp:12`, `Application.hpp:9-15`. Verification: no test.

### 3.3 Sensor acquisition

- **FR-7.** ADC1 shall be configured for 12-bit, right-aligned, software-triggered conversion with the fixed sequencer. Sampling time is 1.5 cycles. The channels are IN0, IN1, IN4, IN18, IN21, IN22 on PA0, PA1, PA4, PB1, PB11, PB12. Source: `Core/Src/main.c:179-249`, `Core/Src/stm32c0xx_hal_msp.c:111-127`. Verification: no test.
- **FR-8.** `Adc::init()` shall run `HAL_ADCEx_Calibration_Start` and then `HAL_ADC_Start_DMA` into a 6-element `volatile uint16_t` buffer. Source: `Core/Src/Impl/Hal/Adc.hpp:62-70,94`. Verification: no test. See D-2.
- **FR-9.** `Adc::readAll()` shall copy the six DMA buffer values into a non-volatile array and return it by const reference. Source: `Adc.hpp:78-86`. Verification: no test.

### 3.4 Main loop

- **FR-10.** Each loop iteration shall call `CommunicationManager::process(settings, adc)` with the real `Hal::Adc`. It then busy-waits 10000 iterations of a `volatile int` counter, and repeats forever. Source: `Application.hpp:32-51,56`. Verification: no test.

### 3.5 Protocol handling (transport-independent)

- **FR-11.** `process()` shall handle exactly one 8-byte packet per call, and only when `available()` is true and `receive(buf, 8)` returns true. Otherwise it transmits nothing. Source: `CommunicationManager.hpp:32-41`. Verification: no test (host check: no packet / failed receive gives no transmit).
- **FR-12.** The system shall decode request byte 0 as: bit 0 = write flag (1 = write), bits 1-7 = command ID. Source: `CommunicationManager.hpp:55-57`. Verification: no test (host check).
- **FR-13.** For every handled packet, the system shall transmit an 8-byte response. Byte 0 echoes request byte 0, and all other bytes are 0 unless a read fills them. Source: `CommunicationManager.hpp:59-61,145`. Verification: no test (host check).
- **FR-14.** Command 0x00 read shall return ADC channels 0, 1 and 2 as little-endian 16-bit values in bytes 1-6. Byte 7 is 0. A write to 0x00 shall change nothing. Source: `CommunicationManager.hpp:64-86`. Verification: no test (host check: `00 BC 0A 23 01 FF 0F 00` for channels 0x0ABC, 0x0123, 0x0FFF).
- **FR-15.** Commands 0x01, 0x02, 0x03 and 0x05 shall read or write `transparentThreshold`, `nontransparentThreshold`, `timeDelay` and `deviceId` respectively. The value is little-endian in bytes 1-2. A read returns the value; a write stores it and responds with echo + zeros. Source: `CommunicationManager.hpp:88-113,130-137`. Verification: no test (host check: read 0x01 default gives `02 E8 03 ..`; write `03 D0 07 ..` stores 2000 and responds `03 00 ..`; read 0x05 default gives `0A 23 01 ..`).
- **FR-16.** Written values shall be stored without range or consistency checks. `Settings::isValid()` exists but is not called anywhere. Source: `CommunicationManager.hpp:90,99,108,132`, `Settings.hpp:59-63`. Verification: no test (host check: `timeDelay = 0` and `nontransparentThreshold < transparentThreshold` are both accepted).
- **FR-17.** Command 0x04 read shall return in byte 1 a bit mask in which bit *i* (i = 0..5) is set when ADC channel *i* is strictly greater than `nontransparentThreshold`. A write to 0x04 shall change nothing. Source: `CommunicationManager.hpp:115-128`. Verification: no test (host check: channel value 3000 with threshold 3000 is not set).
- **FR-18.** An unknown command ID shall produce the echo + zeros response, with no error code. Source: `CommunicationManager.hpp:139-141,145`. Verification: no test (host check).

### 3.6 Settings

- **FR-19.** Settings shall start with these defaults: `transparentThreshold` 1000, `nontransparentThreshold` 3000, `timeDelay` 100, `deviceId` 0x123, `filterId` 0x000, `emptyValue` 500. Source: `Core/Src/Impl/App/Settings.hpp:36-41`. Verification: no test (host check for the first four).
- **FR-20.** `Settings::isValid()` shall return true only if `transparentThreshold < nontransparentThreshold`, `timeDelay > 0` and `emptyValue < transparentThreshold`. Source: `Settings.hpp:59-63`. Verification: no test (host check).

### 3.7 UART transport (USART1)

- **FR-21.** USART1 shall run at 115200 baud, 8 data bits, no parity, 1 stop bit, no flow control, FIFO disabled, on PC4 (TX) / PC5 (RX), with its IRQ enabled at priority 0. Source: `Core/Src/main.c:314-344`, `Core/Src/stm32c0xx_hal_msp.c:298-311`. Verification: no test.
- **FR-22.** `UartTransport::init()` shall log `UartTransport: Initialized (Interrupt mode)` and start interrupt reception of 8 bytes. Source: `Core/Src/Impl/Drivers/Transport/UartTransport.hpp:66-71`. Verification: no test.
- **FR-23.** `HAL_UART_RxCpltCallback` shall call `onRxComplete()` only for USART1 and only when `g_uartTransport` is non-null. `onRxComplete()` sets the ready flag with release ordering and logs `UartTransport: Packet received via interrupt`. Source: `Core/Src/Impl/main.cpp:68-72`, `UartTransport.hpp:135-138`. Verification: no test. See D-1: the pointer is never set.
- **FR-24.** `receive()` shall return false if the ready flag is clear. Otherwise it copies `min(length, 8)` bytes, logs `RX[n]: ..` in hex, clears the flag and re-arms 8-byte interrupt reception. Source: `UartTransport.hpp:108-128`. Verification: no test.
- **FR-25.** `transmit()` shall log `UartTransport: Transmitting packet...`, send the buffer on USART1 with a blocking 100 ms timeout, and log `TX[n]: ..` in hex. Source: `UartTransport.hpp:79-90`. Verification: no test.

### 3.8 CAN transport

- **FR-26.** `CanTransport` shall act as a stub: `init()` returns true, `available()` and `receive()` return false, `transmit()` does nothing. As a result, the CAN build processes no packets. Source: `Core/Src/Impl/Drivers/Transport/CanTransport.hpp:11-32`. Verification: no test.
- **FR-27.** FDCAN1 shall be initialised with classic frames, normal mode, nominal prescaler 12, SJW 1, seg1 3, seg2 1, one standard filter, on PD0 (RX) / PD1 (TX). CubeMX computes 800000 bit/s for these values. The peripheral is not started anywhere in the code. Source: `Core/Src/main.c:271-292`, `Core/Src/stm32c0xx_hal_msp.c:221-230`, `OpticManager.ioc:29-35`. Verification: no test.

### 3.9 Debug output (USART2)

- **FR-28.** USART2 shall run at 115200 baud, 8N1, on PA2 (VCP_TX) / PA3 (VCP_RX). Source: `Core/Src/main.c:364-378`, `Core/Src/stm32c0xx_hal_msp.c:326-335`. Verification: no test.
- **FR-29.** `Logger::log(msg)` shall send the message and then `\r\n` on USART2. Each part uses a blocking transmit with a 100 ms timeout. Source: `Core/Src/Impl/Utils/Logger.hpp:56-66`. Verification: no test.
- **FR-30.** `Logger::log(prefix, value)` shall format `prefix: value` into a 128-byte buffer and drop the message if formatting fails or is truncated. Source: `Logger.hpp:48,84-94`. Verification: no test.
- **FR-31.** `_write()` shall send stdout/stderr data to USART2 with a 100 ms timeout and return `len` unconditionally. Source: `Core/Src/Impl/main.cpp:50-54`. Verification: no test.

### 3.10 Test support

- **FR-32.** `Hal::AdcStub::readAll()` shall return 3000 on all six channels when its call counter mod 20 is greater than 10, and 500 otherwise. The firmware does not use it (`Application` holds `Hal::Adc`). Source: `Core/Src/Impl/Hal/AdcStub.hpp:29-43`, `Application.hpp:56`. Verification: no test (host check: 9 of 20 calls return 3000).
- **FR-33.** The Arduino tester shall repeat this cycle: read 0x00, read 0x04, write 0x01 = 2000, read 0x01. It uses SoftwareSerial (RX pin 10, TX pin 11) at 115200, waits up to 1000 ms for 8 bytes, and prints them in hex, or `Timeout waiting for response`, on `Serial` at 115200. Source: `Tests/ArduinoTester/ArduinoTester.ino:16-92`. Verification: manual (no record of a run in the repository).

### 3.11 Declared but not implemented

| Claim | Where claimed | State in code |
|---|---|---|
| Hysteresis and debouncing of sensor states | `README.md:12`, `Settings.hpp:25-29`, `ARCHITECTURE.md:220-223` | `timeDelay` is stored and served over the protocol but read nowhere else. `Application.hpp:36` is `TODO` |
| EMPTY / FULL / EMPTY_TEMPORARY state machine | `ARCHITECTURE.md:176-199` | Not present. 0x04 uses one threshold (`CommunicationManager.hpp:121-125`) |
| CAN communication | `README.md:8,33` | Stub (`CanTransport.hpp:11-32`). FDCAN1 initialised but not started |
| LED update | `Application.hpp:44` (`TODO`) | LEDs only set high at init (`main.c:421,424`) |
| Continuous DMA sampling | `README.md:7`, `Adc.hpp:7-8,25-36` | Configuration is single conversion / DMA normal. See D-2 |
| Settings consistency check | `Settings.hpp:50-58` | `isValid()` is never called |
| Runtime change of all settings via protocol | `Settings.hpp:22-23` | `filterId` and `emptyValue` have no command |
| "Enable verbose logging in UartTransport.hpp" | `README.md:145` | There is no switch. TX/RX logging is always on (`UartTransport.hpp:80-89,117-121`) |
| "Thread-safe logging" | `Logger.hpp:27` | No locking. `Logger::log` is called from ISR (`UartTransport.hpp:137`) and from the main loop |
| Protocol document `CAN_protocol (1).docx` | `README.md:151` | Not in the repository |
| Entropy source for `std::random_device` | `main.cpp:23-26` | `_getentropy` is a stub that returns 0 and fills nothing |

## 4. Interfaces

### 4.1 Protocol packet (8 bytes, both directions)

| Byte | Request | Response |
|---|---|---|
| 0 | bit 0: 1 = write, 0 = read; bits 1-7: command ID | copy of request byte 0 |
| 1-2 | write value, little-endian (commands 0x01-0x03, 0x05) | read value, little-endian; or 0 |
| 3-6 | ignored | 0 (except 0x00: channel 1 and 2 values) |
| 7 | ignored | 0 |

Source: `CommunicationManager.hpp:34,55-145`, `UartTransport.hpp:52`.

| Cmd | Byte 0 read / write | Meaning | R/W in code |
|---|---|---|---|
| 0x00 | 0x00 / 0x01 | ADC channels 0-2 | read only |
| 0x01 | 0x02 / 0x03 | `transparentThreshold` | R/W |
| 0x02 | 0x04 / 0x05 | `nontransparentThreshold` | R/W |
| 0x03 | 0x06 / 0x07 | `timeDelay` | R/W |
| 0x04 | 0x08 / 0x09 | sensor state bit mask (byte 1) | read only |
| 0x05 | 0x0A / 0x0B | `deviceId` | R/W |

### 4.2 Pins and peripherals

| Pin | Function | Use | Source |
|---|---|---|---|
| PA0, PA1, PA4, PB1, PB11, PB12 | ADC1_IN0, IN1, IN4, IN18, IN21, IN22 | sensor inputs | `stm32c0xx_hal_msp.c:111-127` |
| PC4 / PC5 | USART1 TX / RX | protocol | `stm32c0xx_hal_msp.c:298-307` |
| PA2 / PA3 | USART2 TX / RX (VCP) | debug log | `stm32c0xx_hal_msp.c:326-335`, `main.h:70-73` |
| PD0 / PD1 | FDCAN1 RX / TX | CAN (unused) | `stm32c0xx_hal_msp.c:221-230` |
| PD2 | GPIO out, CAN_STBY | driven low at init | `main.c:427`, `main.h:82-83` |
| PA5 / PC9 | GPIO out, LED_GREEN / LED_BLUE | driven high at init | `main.c:421-424`, `main.h:74-81` |
| PC13 | GPIO in, pull-up, USER_BTN | not read | `main.c:430-433` |
| PA13 / PA14 | SWDIO / SWCLK | debug | `main.h:76-79` |

**Inference (fixed sequencer, upward scan, not checked on hardware):** DMA buffer index 0..5
corresponds to IN0, IN1, IN4, IN18, IN21, IN22, which is the ascending channel order.
`Adc.hpp:50` says the rank order must match the array index.

### 4.3 Interrupts

| IRQ | Priority | Handler | Source |
|---|---|---|---|
| USART1 | 0 | `HAL_UART_IRQHandler(&huart1)` -> weak-override `HAL_UART_RxCpltCallback` | `main.c:343`, `stm32c0xx_it.c:161-170`, `main.cpp:68` |
| DMA1_Channel1 | 0 | `HAL_DMA_IRQHandler(&hdma_adc1)` | `main.c:396-397`, `stm32c0xx_it.c:147-156` |
| SysTick | 3 | `HAL_IncTick()` | `stm32c0xx_hal_conf.h:150`, `OpticManager.ioc:89` |

HAL register-callbacks are disabled (`USE_HAL_UART_REGISTER_CALLBACKS 0U`, `stm32c0xx_hal_conf.h:83`).

### 4.4 Debug log messages (USART2, each ends with `\r\n`)

`Initializing Application...`, `Initialization Complete.` (`Application.hpp:23,26`);
`UartTransport: Initialized (Interrupt mode)` (`UartTransport.hpp:67`);
`UartTransport: Packet received via interrupt` (`UartTransport.hpp:137`);
`RX[8]: xx xx xx xx xx xx xx xx` (`UartTransport.hpp:119`);
`UartTransport: Transmitting packet...`, `TX[8]: xx ..` (`UartTransport.hpp:80,86`).

### 4.5 Build-time switch

| Name | Where | Default | Effect |
|---|---|---|---|
| `USE_UART_TRANSPORT` | `Core/Src/Impl/main.cpp:12` | defined | UART transport. When undefined, the CAN stub is used (`Application.hpp:9-15`) |

There are no environment variables, CLI flags or runtime configuration files.

## 5. Data

| Entity | Fields | Storage | Source |
|---|---|---|---|
| `App::Settings` | `transparentThreshold`, `nontransparentThreshold`, `timeDelay`, `deviceId`, `filterId`, `emptyValue` (all `uint16_t`) | RAM, member of the static `Application`. Nothing writes it to flash, so values return to the defaults after reset (inference from the absence of any flash-program call in `Core/Src/Impl`) | `Settings.hpp:34-64`, `Application.hpp:57` |
| ADC readings | 6 x `uint16_t`, 0-4095 (12-bit) | `dmaBuffer_` (volatile, DMA target) and `readings_` (copy) | `Adc.hpp:94,100`, `main.c:181` |
| UART RX buffer | 8 bytes + `std::atomic<bool> rxReady_` | `UartTransport` member | `UartTransport.hpp:141-142` |
| Protocol frames | 8-byte stack arrays `rxBuffer`, `txBuffer` | stack of `process` / `handlePacket` | `CommunicationManager.hpp:34,59` |

Units: `timeDelay` is documented as milliseconds (`Settings.hpp:38`). Thresholds have no stated
unit. **Inference:** they are raw ADC counts, because 0x04 compares them directly with ADC values
(`CommunicationManager.hpp:122`).

## 6. Non-functional requirements

- **NFR-1.** Packet size is fixed at 8 bytes. Source: `UartTransport.hpp:52`, `CommunicationManager.hpp:34`.
- **NFR-2.** Every UART transmit blocks with a 100 ms timeout (protocol TX, log, `_write`). Source: `UartTransport.hpp:81`, `Logger.hpp:58-65`, `main.cpp:52`.
- **NFR-3.** ISR-to-main-loop handoff uses `std::atomic<bool>` with acquire/release ordering. Source: `UartTransport.hpp:98,109,124,136,142`.
- **NFR-4.** The application object is statically allocated. `Core/Src/Impl` contains no `new`, `malloc`, `std::vector` or `std::string` (searched 2026-11-18). Minimum heap is 0x200 and minimum stack is 0x400. Source: `main.cpp:84`, `STM32C092RCTX_FLASH.ld:41-42`, `OpticManager.ioc:186,198`.
- **NFR-5.** Formatted log lines are limited to 128 bytes and hex dumps to a 64-byte buffer. Source: `Logger.hpp:48`, `UartTransport.hpp:84,118`.
- **NFR-6.** Interrupt priorities: USART1 0, DMA1_Channel1 0, SysTick 3. Source: see 4.3.
- **NFR-7.** No watchdog: the IWDG and WWDG HAL modules are disabled. Source: `stm32c0xx_hal_conf.h:43,54`.
- **NFR-8.** Main-loop pacing is a busy loop of 10000 iterations, with no time base. Source: `Application.hpp:48-50`.
- **NFR-9.** The Arduino tester waits at most 1000 ms for a response. Source: `ArduinoTester.ino:51`.

## 7. Constraints and dependencies

| Dependency | Version | Source |
|---|---|---|
| STM32CubeMX project | MX 6.15.0, DB 6.0.150 | `OpticManager.ioc:81-82` |
| STM32Cube firmware package | FW_C0 V1.4.0 | `OpticManager.ioc:183` |
| STM32C0xx HAL driver (vendored) | 1.4.0 | `Drivers/STM32C0xx_HAL_Driver/Src/stm32c0xx_hal.c:58-61` |
| CMSIS Core (vendored) | 5.6 | `Drivers/CMSIS/Include/cmsis_version.h:35-36` |
| IDE / toolchain | STM32CubeIDE (target toolchain), GCC; README states CubeIDE 1.19.0+ | `OpticManager.ioc:175,199`, `README.md:82-84` |
| C++ standard library | C++20 headers `<ranges>`, `<atomic>`, `<string_view>`; `std::span` is used (line 85) but `<span>` is not included directly | `UartTransport.hpp:13-18,85`, `Logger.hpp:17` |
| Documentation | Doxygen 1.13.2 (version that generated `docs/html`) | `docs/html/index.html` |
| Tester | Arduino Uno, `SoftwareSerial` library | `ArduinoTester.ino:3,13` |

The STM32CubeIDE project files (`.project`, `.cproject`, `.mxproject`) and `*.launch` are
listed in `.gitignore:5-7,22` and are not tracked. Compiler flags therefore cannot be read from
the repository (see 9).

Licence: `README.md:163` states "Proprietary - All rights reserved". The vendored ST and CMSIS code
carries its own `LICENSE.txt` (`Drivers/CMSIS/LICENSE.txt`,
`Drivers/STM32C0xx_HAL_Driver/LICENSE.txt`, `Drivers/CMSIS/Device/ST/STM32C0xx/LICENSE.txt`).

## 8. Acceptance

### 8.1 Existing checks

There are no unit tests and no CI configuration in this commit (`git ls-files` at `5feba9c`).
The only test artefact is the manual Arduino sketch (FR-33).

### 8.2 Manual check on hardware (procedure from `README.md:104-114`)

1. Wire Arduino pin 11 to PC5, pin 10 to PC4, and GND to GND.
2. Flash the firmware. Open the Arduino serial monitor at 115200, and the NUCLEO VCP at 115200.
3. Expected per this spec: responses `00 ..` (FR-14), `08 <mask> ..` (FR-17), `03 00 ..`, then
   `02 D0 07 ..` (FR-15). Expected log lines per 4.4.
4. Because of D-1, the likely outcome at this commit is `Timeout waiting for response` for every
   request, including the first. This is an inference; no run is recorded.

### 8.3 Host check of the protocol logic (performed 2026-11-18, outside the repository)

A harness in a temporary directory outside the repository included `CommunicationManager.hpp`,
with a mock transport and a fake ADC. It was compiled with `clang++ -std=c++20` and run: 23 of 23
checks passed. They covered FR-11 to FR-20 and FR-32. This does not exercise HAL, interrupts, DMA
or the target compiler.

## 9. Open questions

1. What is the device for: which liquids, tubes or process do "transparent / nontransparent liquid sensitivity" refer to? Who operates it?
2. The protocol document `CAN_protocol (1).docx` (`README.md:151`) is not in the repository. What should 0x00 return for six channels (`CommunicationManager.hpp:67-78` records the same doubt)? Are there error or NACK responses? What are the CAN IDs, and how do `deviceId` and `filterId` take part in addressing?
3. Is D-1 known? Has the UART build ever answered the Arduino tester on hardware?
4. Is single-shot ADC sampling (D-2) intended? The comments say continuous.
5. Which compiler flags are used (`-std=gnu++20`, `-fno-exceptions`, `-fno-rtti` per `README.md:125-126`, `ARCHITECTURE.md:307`)? Which optimisation level? `.cproject` is not tracked. The `.ioc` records `ProjectManager.CompilerOptimize=6` (`OpticManager.ioc:176`); the repository does not say which `-O` level this means.
6. How is the project built from a fresh clone, given that the IDE project files are ignored? Is it regenerated from `OpticManager.ioc`?
7. Must settings survive a reset (flash storage)?
8. What is the CAN transceiver, and what is the polarity of CAN_STBY (PD2 is driven low)?
9. Is the target only the NUCLEO-C092RC, or is there custom hardware? Which revision?
10. What is the required response time? `ARCHITECTURE.md:348` says "~2ms", which is not measured. What should the main-loop period be?
11. What should the LEDs and the user button do?
12. The memory figures in `README.md:116-120` and `ARCHITECTURE.md:256-276` (~15 KB flash, 128 KB / 12 KB parts) do not match the linker script (256 KB / 30 KB). What is the real image size?
13. How does `dev` (this commit) relate to `origin/release`, which is 17 commits ahead and adds tests, CI and other documents that are not described here?
