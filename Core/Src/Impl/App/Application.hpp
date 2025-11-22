#pragma once

#include "../Drivers/Transport/CommunicationManager.hpp"
#include "../Hal/Adc.hpp"
#include "../Utils/Logger.hpp"
#include "Settings.hpp"

// Select Transport Policy here or via macro
#ifdef USE_UART_TRANSPORT
	#include "../Drivers/Transport/UartTransport.hpp"
	using TransportPolicy = Drivers::Transport::UartTransport;
#else
    #include "../Drivers/Transport/CanTransport.hpp"
    using TransportPolicy = Drivers::Transport::CanTransport;
#endif

namespace App {

template <typename Transport = TransportPolicy>
class Application {
public:
    void init() {
        Utils::Logger::log("Initializing Application...");
        comms_.init();
        adc_.init();
        Utils::Logger::log("Initialization Complete.");
    }

    void run() {
        init();

        while (true) {
            // 1. Read Sensors
            // auto& readings = adc_.readAll(); // Done inside process for now or separate logic

            // 2. Process Logic (Debounce, Thresholds) - TODO

            // 3. Handle Communication
            // Note: CommunicationManager needs to be updated to accept Real Adc if it was templated or hardcoded to Stub.
            // Currently CommunicationManager expects AdcStub. I need to update it or make it a template.
            // For now, I will cast or change CommunicationManager to accept Hal::Adc.
            comms_.process(settings_, adc_);

            // 4. Update LEDs - TODO
            
            // Simple delay loop stub
            // Simple delay loop stub
            for(volatile int i=0; i<10000;) {
                i = i + 1;
            }
        }
    }

private:
    Drivers::Transport::CommunicationManager<Transport> comms_;
    Hal::Adc adc_;
    App::Settings settings_;
};

} // namespace App
