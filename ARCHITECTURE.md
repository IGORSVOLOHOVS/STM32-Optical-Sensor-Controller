# OpticManager Architecture

This document provides visual diagrams to understand the OpticManager project architecture.

## System Architecture

```mermaid
graph TB
    subgraph "Hardware Layer"
        ADC[ADC + DMA<br/>6 Channels]
        UART1[USART1<br/>PC4/PC5]
        UART2[USART2<br/>PA2/PA3 VCP]
        CAN[FDCAN1]
    end
    
    subgraph "HAL Layer"
        AdcDriver[Adc.hpp<br/>DMA Driver]
        AdcStub[AdcStub.hpp<br/>Test Stub]
    end
    
    subgraph "Transport Layer"
        UartTransport[UartTransport.hpp<br/>Interrupt-based]
        CanTransport[CanTransport.hpp<br/>Stub]
        CommMgr[CommunicationManager.hpp<br/>Protocol Handler]
    end
    
    subgraph "Application Layer"
        App[Application.hpp<br/>Main Logic]
        Settings[Settings.hpp<br/>Configuration]
    end
    
    subgraph "Utilities"
        Logger[Logger.hpp<br/>Debug Output]
    end
    
    ADC --> AdcDriver
    AdcDriver --> App
    AdcStub -.-> App
    
    UART1 --> UartTransport
    CAN --> CanTransport
    UartTransport --> CommMgr
    CanTransport -.-> CommMgr
    
    CommMgr --> App
    Settings --> App
    Settings --> CommMgr
    
    UART2 --> Logger
    App --> Logger
    UartTransport --> Logger
    
    style App fill:#4CAF50
    style CommMgr fill:#2196F3
    style AdcDriver fill:#FF9800
    style UartTransport fill:#9C27B0
```

## Class Diagram

```mermaid
classDiagram
    class Application~Transport~ {
        -CommunicationManager~Transport~ comms_
        -Adc adc_
        -Settings settings_
        +init() void
        +run() void
    }
    
    class CommunicationManager~TransportPolicy~ {
        -TransportPolicy transport_
        +init() bool
        +process(Settings&, AdcType&) void
        +getTransport() TransportPolicy&
        -handlePacket(uint8_t*, Settings&, AdcType&) void
    }
    
    class UartTransport {
        -array~uint8_t, 8~ rxBuffer_
        -atomic~bool~ rxReady_
        +init() bool
        +transmit(uint8_t*, size_t) void
        +available() bool
        +receive(uint8_t*, size_t) bool
        +onRxComplete() void
    }
    
    class CanTransport {
        +init() bool
        +transmit(uint8_t*, size_t) void
        +available() bool
        +receive(uint8_t*, size_t) bool
    }
    
    class Adc {
        -volatile uint16_t[6] dmaBuffer_
        -array~uint16_t, 6~ readings_
        +init() void
        +readAll() array~uint16_t, 6~&
    }
    
    class AdcStub {
        -array~uint16_t, 6~ readings_
        +init() void
        +readAll() array~uint16_t, 6~&
    }
    
    class Settings {
        +uint16_t transparentThreshold
        +uint16_t nontransparentThreshold
        +uint16_t timeDelay
        +uint16_t deviceId
        +isValid() bool
    }
    
    class Logger {
        <<static>>
        +log(string_view) void
        +log(string_view, int) void
    }
    
    Application *-- CommunicationManager
    Application *-- Adc
    Application *-- Settings
    Application ..> Logger
    
    CommunicationManager *-- UartTransport
    CommunicationManager *-- CanTransport
    CommunicationManager ..> Settings
    CommunicationManager ..> Adc
    CommunicationManager ..> AdcStub
    
    UartTransport ..> Logger
```

## UART Communication Sequence

```mermaid
sequenceDiagram
    participant Arduino
    participant USART1 as USART1 HW
    participant ISR as UART ISR
    participant Transport as UartTransport
    participant CommMgr as CommunicationManager
    participant App as Application
    participant ADC as Adc
    
    Note over Arduino,ADC: Read ADC Command (0x00)
    
    Arduino->>USART1: Send [0x00, 0x00, ...]
    USART1->>ISR: RX Complete Interrupt
    ISR->>Transport: onRxComplete()
    Transport->>Transport: Set rxReady_ = true
    
    loop Main Loop
        App->>CommMgr: process(settings, adc)
        CommMgr->>Transport: available()
        Transport-->>CommMgr: true
        CommMgr->>Transport: receive(buffer, 8)
        Transport-->>CommMgr: [0x00, 0x00, ...]
        
        CommMgr->>CommMgr: handlePacket()
        CommMgr->>ADC: readAll()
        ADC-->>CommMgr: [val0, val1, ..., val5]
        
        CommMgr->>CommMgr: Pack response
        CommMgr->>Transport: transmit(response, 8)
        Transport->>USART1: HAL_UART_Transmit()
        USART1->>Arduino: Response data
    end
```

## Sensor State Machine

```mermaid
stateDiagram-v2
    [*] --> EMPTY
    
    EMPTY --> FULL: ADC > nontransparentThreshold
    FULL --> EMPTY_TEMPORARY: ADC < transparentThreshold
    EMPTY_TEMPORARY --> EMPTY: Delay expired
    EMPTY_TEMPORARY --> FULL: ADC > nontransparentThreshold
    
    note right of EMPTY
        Sensor detects no liquid
        ADC < transparentThreshold
    end note
    
    note right of FULL
        Sensor detects liquid
        ADC > nontransparentThreshold
    end note
    
    note right of EMPTY_TEMPORARY
        Temporary empty state
        Debouncing with timeDelay
    end note
```

## Data Flow Diagram

```mermaid
flowchart LR
    subgraph Sensors
        S1[Sensor 1]
        S2[Sensor 2]
        S3[Sensor 3]
        S4[Sensor 4]
        S5[Sensor 5]
        S6[Sensor 6]
    end
    
    subgraph ADC_DMA["ADC + DMA"]
        ADC[12-bit ADC<br/>Continuous Mode]
        DMA[DMA Circular<br/>Buffer]
    end
    
    subgraph Processing
        Read[Read DMA Buffer]
        Threshold[Apply Thresholds]
        Debounce[Debouncing Logic]
        State[Determine States]
    end
    
    subgraph Communication
        Protocol[Protocol Handler]
        UART[UART TX/RX]
        CAN[CAN TX/RX]
    end
    
    subgraph External
        RaspberryPi[Raspberry Pi<br/>Master]
    end
    
    S1 & S2 & S3 & S4 & S5 & S6 --> ADC
    ADC --> DMA
    DMA --> Read
    Read --> Threshold
    Threshold --> Debounce
    Debounce --> State
    State --> Protocol
    Protocol --> UART
    Protocol -.-> CAN
    UART <--> RaspberryPi
    CAN <-.-> RaspberryPi
    
    style ADC fill:#FF9800
    style DMA fill:#FF9800
    style Protocol fill:#2196F3
    style State fill:#4CAF50
```

## Memory Layout

```mermaid
graph TB
    subgraph Flash["Flash Memory (128KB)"]
        Code[Program Code<br/>~15KB]
        Const[Const Data<br/>~1KB]
        Free1[Free Space<br/>~112KB]
    end
    
    subgraph SRAM["SRAM (12KB)"]
        Stack[Stack<br/>~1KB]
        Globals[Global Variables<br/>~500B]
        DMABuf[DMA Buffer<br/>12 bytes]
        UARTBuf[UART RX Buffer<br/>8 bytes]
        AppData[Application Data<br/>~500B]
        Free2[Free Space<br/>~10KB]
    end
    
    style Code fill:#4CAF50
    style DMABuf fill:#FF9800
    style UARTBuf fill:#9C27B0
```

## Interrupt Priority

```mermaid
graph LR
    subgraph "Interrupt Priorities (0 = Highest)"
        P0[Priority 0<br/>USART1 RX]
        P1[Priority 1<br/>DMA ADC]
        P2[Priority 2<br/>SysTick]
    end
    
    P0 -.-> ISR1[UART ISR<br/>onRxComplete]
    P1 -.-> ISR2[DMA ISR<br/>Transfer Complete]
    P2 -.-> ISR3[SysTick ISR<br/>HAL Tick]
    
    style P0 fill:#f44336
    style P1 fill:#FF9800
    style P2 fill:#4CAF50
```

## Build Process

```mermaid
flowchart TD
    Start([Start Build])
    
    Config{Transport<br/>Selection}
    Config -->|USE_UART_TRANSPORT| UART[Compile with<br/>UartTransport]
    Config -->|Default| CAN[Compile with<br/>CanTransport]
    
    UART --> Compile[Compile C++20<br/>-std=gnu++20]
    CAN --> Compile
    
    Compile --> Link[Link with<br/>HAL Libraries]
    Link --> Binary[Generate<br/>.elf, .hex, .bin]
    Binary --> Flash[Flash to<br/>STM32]
    Flash --> End([Ready])
    
    style Config fill:#2196F3
    style Compile fill:#4CAF50
    style Flash fill:#FF9800
```

## Protocol Timing Diagram

```mermaid
sequenceDiagram
    autonumber
    participant Master as Raspberry Pi
    participant UART as USART1
    participant STM32
    participant ADC as ADC+DMA
    
    Note over Master,ADC: Command: Read Sensor States (0x04)
    
    Master->>UART: TX: [0x08, 0x00, ...]
    Note right of Master: Cmd=0x04, R/W=0 (Read)
    
    UART->>STM32: RX Interrupt
    activate STM32
    STM32->>STM32: Parse command
    STM32->>ADC: readAll()
    ADC-->>STM32: [1234, 2345, ...]
    STM32->>STM32: Apply thresholds
    STM32->>STM32: Pack states byte
    STM32->>UART: TX: [0x08, 0x3F, ...]
    deactivate STM32
    Note right of STM32: States = 0b00111111
    
    UART-->>Master: RX: Response
    
    Note over Master,ADC: Total time: ~2ms
```

## Component Interaction Diagram

```mermaid
graph TB
    subgraph "External Systems"
        RPI[Raspberry Pi<br/>Master Controller]
        Arduino[Arduino Tester<br/>Development Tool]
    end
    
    subgraph "STM32 Firmware"
        subgraph "Application Layer"
            App[Application<br/>Main Loop]
            Settings[Settings<br/>Configuration]
        end
        
        subgraph "Communication Layer"
            CommMgr[CommunicationManager<br/>Protocol Logic]
            UART[UartTransport<br/>Interrupt RX/TX]
            CAN[CanTransport<br/>Alternative]
        end
        
        subgraph "Hardware Abstraction"
            ADC[Adc Driver<br/>DMA Continuous]
            Logger[Logger<br/>Debug UART]
        end
    end
    
    subgraph "Hardware"
        Sensors[6x Optical<br/>Sensors]
        USART1_HW[USART1 HW<br/>PC4/PC5]
        USART2_HW[USART2 HW<br/>PA2/PA3 VCP]
        ADC_HW[ADC1 HW<br/>6 Channels]
    end
    
    RPI <-->|Protocol| USART1_HW
    Arduino <-->|Test Commands| USART1_HW
    
    USART1_HW <--> UART
    USART2_HW <--> Logger
    ADC_HW <--> ADC
    Sensors --> ADC_HW
    
    UART <--> CommMgr
    CAN -.-> CommMgr
    CommMgr <--> App
    Settings <--> App
    Settings <--> CommMgr
    ADC <--> App
    
    App --> Logger
    UART --> Logger
    
    style App fill:#4CAF50,stroke:#2E7D32,stroke-width:3px
    style CommMgr fill:#2196F3,stroke:#1565C0,stroke-width:3px
    style UART fill:#9C27B0,stroke:#6A1B9A,stroke-width:3px
    style ADC fill:#FF9800,stroke:#E65100,stroke-width:3px
```

## Deployment Diagram

```mermaid
graph TB
    subgraph "Development Environment"
        IDE[STM32CubeIDE<br/>Windows]
        GCC[ARM GCC<br/>Toolchain]
        Doxygen[Doxygen<br/>Documentation]
    end
    
    subgraph "Target Hardware"
        Nucleo[NUCLEO-C092RC<br/>Development Board]
        STM32[STM32C092RC<br/>MCU]
        STLINK[ST-LINK/V2-1<br/>Programmer/Debugger]
        VCP[Virtual COM Port<br/>USART2]
    end
    
    subgraph "External Devices"
        Sensors[6x Optical Sensors<br/>Analog Output]
        RPI[Raspberry Pi<br/>UART/CAN Master]
        Arduino[Arduino Uno<br/>Test Tool]
        PC[PC<br/>Serial Monitor]
    end
    
    IDE -->|Build| GCC
    GCC -->|.elf/.hex| STLINK
    STLINK -->|Flash| STM32
    STLINK <-->|Debug| IDE
    
    STM32 <-->|VCP| PC
    STM32 <-->|USART1| RPI
    STM32 <-->|USART1| Arduino
    STM32 <-->|ADC| Sensors
    
    Doxygen -->|Generate| Docs[HTML Docs]
    
    style STM32 fill:#4CAF50,stroke:#2E7D32,stroke-width:4px
    style Nucleo fill:#E3F2FD,stroke:#1976D2,stroke-width:2px
    style RPI fill:#FFF3E0,stroke:#F57C00,stroke-width:2px
```

## Template Instantiation Diagram

```mermaid
graph LR
    subgraph "Template Classes"
        AppTemplate["Application&lt;Transport&gt;"]
        CommTemplate["CommunicationManager&lt;TransportPolicy&gt;"]
    end
    
    subgraph "Concrete Types"
        UartTransport[UartTransport]
        CanTransport[CanTransport]
    end
    
    subgraph "Instantiated Classes"
        AppUart["Application&lt;UartTransport&gt;"]
        AppCan["Application&lt;CanTransport&gt;"]
        CommUart["CommunicationManager&lt;UartTransport&gt;"]
        CommCan["CommunicationManager&lt;CanTransport&gt;"]
    end
    
    AppTemplate -.->|USE_UART_TRANSPORT| AppUart
    AppTemplate -.->|Default| AppCan
    CommTemplate -.->|USE_UART_TRANSPORT| CommUart
    CommTemplate -.->|Default| CommCan
    
    UartTransport -.-> AppUart
    UartTransport -.-> CommUart
    CanTransport -.-> AppCan
    CanTransport -.-> CommCan
    
    style AppUart fill:#4CAF50,stroke:#2E7D32,stroke-width:3px
    style CommUart fill:#2196F3,stroke:#1565C0,stroke-width:3px
```

## Error Handling Flow

```mermaid
flowchart TD
    Start([Function Call])
    
    Init{Initialization<br/>Successful?}
    Start --> Init
    
    Init -->|No| Error1[Log Error]
    Init -->|Yes| Operation[Perform Operation]
    
    Operation --> Check{Operation<br/>Valid?}
    
    Check -->|Invalid| Error2[Return false/error]
    Check -->|Valid| Process[Process Data]
    
    Process --> Validate{Data<br/>Valid?}
    
    Validate -->|No| Error3[Discard/Log]
    Validate -->|Yes| Success[Return Success]
    
    Error1 --> ErrorHandler[Error Handler]
    Error2 --> ErrorHandler
    Error3 --> Retry{Retry<br/>Possible?}
    
    Retry -->|Yes| Operation
    Retry -->|No| ErrorHandler
    
    ErrorHandler --> End([End])
    Success --> End
    
    style Error1 fill:#f44336
    style Error2 fill:#f44336
    style Error3 fill:#f44336
    style Success fill:#4CAF50
```

