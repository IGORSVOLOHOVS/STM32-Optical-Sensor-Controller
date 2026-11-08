# STM32 Optical Sensor Controller

Firmware in C++20 for the STM32C092RC that reads six optical liquid sensors and
answers an 8-byte command protocol over UART or CAN. The protocol handler,
`CommunicationManager`, is header-only and takes its transport as a template
policy, so the PC builds and tests it with a fake transport and the simulated ADC.

Status: prototype. The UART transport and the ADC driver are written, the CAN
transport is a stub that never receives, and the sensor logic (hysteresis and
debouncing) is not written yet. The code is from November 2025.

---

## Hardware

| | |
|---|---|
| MCU | STM32C092RC, Cortex-M0+ |
| Board | NUCLEO-C092RC |
| Sensors | 6 optical sensors on analog inputs, sampled by the ADC with DMA |
| Protocol | USART1 on `PC4` (TX) / `PC5` (RX), or FDCAN1 |
| Console | USART2 on `PA2`/`PA3`, 115200 8N1, over the ST-LINK virtual COM port |

The transport is chosen at compile time: `Core/Src/Impl/main.cpp` defines
`USE_UART_TRANSPORT`; without it the application takes `CanTransport`.

---

## Protocol

Every frame is 8 bytes, in both directions.

```
byte 0    : (command << 1) | write     write: 1 = write, 0 = read
byte 1..7 : payload, 16-bit values low byte first
```

| Command | Meaning | Read reply | Write |
|---|---|---|---|
| `0x00` | ADC values | channels 0, 1 and 2 in bytes 1 to 6 | ignored |
| `0x01` | transparent liquid threshold | bytes 1-2 | sets it from bytes 1-2 |
| `0x02` | non-transparent liquid threshold | bytes 1-2 | sets it from bytes 1-2 |
| `0x03` | time delay, ms | bytes 1-2 | sets it from bytes 1-2 |
| `0x04` | sensor states, one bit per channel | byte 1 | ignored |
| `0x05` | device ID | bytes 1-2 | sets it from bytes 1-2 |

What the code does, as the tests record it:

- Every request gets a reply, a known command or not. Byte 0 of the reply is
  byte 0 of the request, and every byte the table does not name is zero, so a
  write is acknowledged by its own echo with zeros.
- Command `0x00` sends only three of the six channels: 8 bytes do not hold six
  12-bit values, and the source says the protocol is not settled there.
- Command `0x04` sets bit `i` when channel `i` reads above
  `nontransparentThreshold`. A reading equal to it is not above it, so with the
  defaults the simulated ADC's 3000 never counts as full.
- `filterId` and `emptyValue` exist in `Settings` but no command reads or
  writes them; `0x06` and up are echoed and change nothing.

Defaults, in `Core/Src/Impl/App/Settings.hpp`: transparent threshold 1000,
non-transparent threshold 3000, time delay 100 ms, device ID `0x123`, filter ID
`0x000`, empty value 500.

---

## Software layout

```
Core/Src/Impl/App/Application.hpp                    main loop: init, then process the transport forever
Core/Src/Impl/App/Settings.hpp                       thresholds, delay, IDs; isValid()
Core/Src/Impl/Drivers/Transport/CommunicationManager.hpp   the protocol, over any transport policy
Core/Src/Impl/Drivers/Transport/UartTransport.hpp    USART1, interrupt-driven receive
Core/Src/Impl/Drivers/Transport/CanTransport.hpp     a stub: available() is always false
Core/Src/Impl/Hal/Adc.hpp                            ADC with DMA, six channels
Core/Src/Impl/Hal/AdcStub.hpp                        simulated ADC: 500 or 3000, toggling every ten reads
Core/Src/Impl/Utils/Logger.hpp                       logging over USART2
Core/Src/Impl/main.cpp                               cpp_main, called from the CubeMX main.c
Core/, Drivers/                                      STM32CubeMX code, the HAL and CMSIS
tests/communication_manager_test.cpp                 host tests
tests/ArduinoTester/ArduinoTester.ino                bench stimulus over UART
OpticManager.ioc                                     the STM32CubeMX project
ARCHITECTURE.md                                      diagrams and design notes
```

`Application.hpp` passes the real `Hal::Adc` to `CommunicationManager::process`,
which is a template over the ADC type, so the same handler runs with
`Hal::AdcStub` on the PC.

---

## Building

The firmware is built by STM32CubeIDE 1.19 or newer: import the folder as an
existing project, then `Project > Build All`, and flash over the board's
ST-LINK. See [docs/install.md](docs/install.md).

The API documentation is generated, not committed: `doxygen Doxyfile` writes
it to `docs/html`.

---

## Tests

`.\recipe.ps1 test` builds `tests/communication_manager_test.cpp` with the
host's g++, which compiles the three headers that need no HAL:
`CommunicationManager.hpp`, `Settings.hpp` and `AdcStub.hpp`. It runs them with
`ctest`, lints them with `clang-tidy`, formats them with `clang-format`, and
checks that `Core/` holds no lock and no exception. The tests use no framework,
so the build downloads nothing. Nothing that includes the HAL is compiled on the
PC.

### Arduino bench tester

`tests/ArduinoTester/ArduinoTester.ino` runs on an Arduino Uno and talks to the
board over a software serial port:

- Arduino pin 11 (TX) to `PC5` (USART1_RX), pin 10 (RX) to `PC4` (USART1_TX), GND to GND.
- Upload the sketch, open the Serial Monitor at 115200 baud, and read the
  command and reply bytes it prints.

---

## Licence

`LICENSE` holds the MIT text, naming Igors Volohovs, added on 2026-08-05. The
README before it said "Proprietary - All rights reserved". The two disagree and
the question is open in this repository's issues; until it is settled, read
`LICENSE` as the file that is there, not as a decision. The files STM32CubeMX
generated and the HAL under `Drivers/` carry ST's own notices.

## Branches

`release` holds what was released and `test` what is being tested. On GitHub
the repository had `main`, which `release` comes from, and `dev`, every commit
of which is in `main`.

## Lab requirements

This repository complies with [revision 4.0.0](http://127.0.0.1:3000/igors-lab/software-requirements/src/tag/v4.0.0/BEST_REQUIREMENTS.md) of the lab requirements, except R-02 (#8), R-09 (#9), R-12 (#10), R-13 (#11), R-13 (#12), R-14 (#13), R-37 (#14), R-41 (#15), R-42 (#16).
