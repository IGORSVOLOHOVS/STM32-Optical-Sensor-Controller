# Manual

## Talking to the board

Send 8-byte frames to USART1 (or FDCAN1 in a CAN build). Byte 0 is `(command << 1) | write`; 16-bit values travel low byte first in bytes 1 and 2. The board answers every frame with 8 bytes whose byte 0 echoes the request.

- Read the device ID: send `0A 00 00 00 00 00 00 00`. The reply is `0A 23 01 00 00 00 00 00` with the default ID `0x123`.
- Set the transparent threshold to 2000: send `03 D0 07 00 00 00 00 00`. The reply is `03 00 00 00 00 00 00 00`.
- Read the sensor states: send `08 00 ...`. Byte 1 of the reply has bit `i` set when channel `i` reads above the non-transparent threshold.

The README's protocol section has the whole table and what each command really does.

## The handler on the PC

`CommunicationManager` takes any transport with `init()`, `available()`, `receive(buffer, length)` and `transmit(data, length)`, and any ADC with `readAll()` returning six readings.

```cpp
#include "Drivers/Transport/CommunicationManager.hpp"
#include "Hal/AdcStub.hpp"

Drivers::Transport::CommunicationManager<MyTransport> comms;
App::Settings settings{};
Hal::AdcStub adc;
comms.init();
comms.process(settings, adc);  // reads one frame if the transport has one, and transmits the reply
```

- `process` does nothing when `available()` is false or `receive` fails.
- `Hal::AdcStub` keeps one counter for every instance: reads 1 to 10 of each 20 return 500 on every channel, reads 11 to 19 return 3000.

## The bench tester

`tests/ArduinoTester/ArduinoTester.ino` sends a fixed sequence of commands from an Arduino Uno and prints each reply on its Serial Monitor at 115200 baud. The README has the wiring.
