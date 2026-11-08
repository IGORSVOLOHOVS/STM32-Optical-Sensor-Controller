// Host tests for CommunicationManager, Settings and AdcStub. No framework: nothing is downloaded, and the exit code is
// the number of failed checks, which is what ctest reads. They record the behaviour as it is, not as the README
// describes it.
#include "App/Settings.hpp"
#include "Drivers/Transport/CommunicationManager.hpp"
#include "Hal/AdcStub.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace {

int failures = 0;

void check(bool ok, const char *what) {
    if (!ok) {
        ++failures;
        std::printf("FAIL: %s\n", what);
    }
}

using Frame = std::array<uint8_t, 8>;

// A transport a test can fill: one pending frame in, every transmitted frame recorded.
struct FakeTransport {
    bool initialised = false;
    bool pending = false;
    bool receiveSucceeds = true;
    Frame incoming{};
    Frame sent{};
    int transmitted = 0;
    size_t lastLength = 0;

    bool init() {
        initialised = true;
        return true;
    }

    bool available() const { return pending; }

    bool receive(uint8_t *buffer, size_t length) {
        pending = false;
        if (!receiveSucceeds) {
            return false;
        }
        for (size_t i = 0; i < length && i < incoming.size(); ++i) {
            buffer[i] = incoming[i];
        }
        return true;
    }

    void transmit(const uint8_t *data, size_t length) {
        ++transmitted;
        lastLength = length;
        for (size_t i = 0; i < length && i < sent.size(); ++i) {
            sent[i] = data[i];
        }
    }
};

// An ADC whose six readings a test sets.
struct FixedAdc {
    std::array<uint16_t, 6> values{};
    int reads = 0;

    const std::array<uint16_t, 6> &readAll() {
        ++reads;
        return values;
    }
};

using Manager = Drivers::Transport::CommunicationManager<FakeTransport>;

template <typename Adc>
Frame exchange(Manager &manager, App::Settings &settings, Adc &adc, const Frame &request) {
    FakeTransport &transport = manager.getTransport();
    transport.incoming = request;
    transport.pending = true;
    transport.sent = Frame{};
    manager.process(settings, adc);
    return transport.sent;
}

constexpr uint8_t readOf(uint8_t command) { return static_cast<uint8_t>(command << 1); }
constexpr uint8_t writeOf(uint8_t command) { return static_cast<uint8_t>((command << 1) | 1); }

void theDefaultSettingsAreTheDocumentedOnes() {
    const App::Settings settings{};
    check(settings.transparentThreshold == 1000, "transparentThreshold defaults to 1000");
    check(settings.nontransparentThreshold == 3000, "nontransparentThreshold defaults to 3000");
    check(settings.timeDelay == 100, "timeDelay defaults to 100");
    check(settings.deviceId == 0x123, "deviceId defaults to 0x123");
    check(settings.filterId == 0x000, "filterId defaults to 0x000");
    check(settings.emptyValue == 500, "emptyValue defaults to 500");
    check(settings.isValid(), "the defaults are valid");
    static_assert(App::DefaultSettings.isValid(), "DefaultSettings is valid at compile time");
}

void isValidRejectsEachBrokenOrdering() {
    App::Settings settings{};
    settings.transparentThreshold = settings.nontransparentThreshold;
    check(!settings.isValid(), "equal thresholds are invalid");
    settings = App::Settings{};
    settings.timeDelay = 0;
    check(!settings.isValid(), "a zero delay is invalid");
    settings = App::Settings{};
    settings.emptyValue = settings.transparentThreshold;
    check(!settings.isValid(), "an empty value at the transparent threshold is invalid");
}

void initReachesTheTransport() {
    Manager manager;
    check(manager.init(), "init returns the transport's answer");
    check(manager.getTransport().initialised, "init initialises the transport");
}

void nothingPendingMeansNothingSent() {
    Manager manager;
    App::Settings settings{};
    FixedAdc adc;
    manager.process(settings, adc);
    check(manager.getTransport().transmitted == 0, "no frame pending, no reply");
    check(adc.reads == 0, "no frame pending, no ADC read");
}

void aFailedReceiveSendsNothing() {
    Manager manager;
    App::Settings settings{};
    FixedAdc adc;
    manager.getTransport().receiveSucceeds = false;
    manager.getTransport().pending = true;
    manager.process(settings, adc);
    check(manager.getTransport().transmitted == 0, "a failed receive gets no reply");
}

void everyReplyIsEightBytesAndEchoesByteZero() {
    Manager manager;
    App::Settings settings{};
    FixedAdc adc;
    for (unsigned first = 0; first <= 0xFF; ++first) {
        const Frame reply = exchange(manager, settings, adc, Frame{static_cast<uint8_t>(first)});
        check(reply[0] == first, "byte 0 of the reply is byte 0 of the request");
        check(manager.getTransport().lastLength == 8, "the reply is 8 bytes");
        check(reply[7] == 0, "byte 7 of the reply is always zero");
    }
    check(manager.getTransport().transmitted == 256, "every request gets a reply, a known command or not");
}

void readingTheAdcPacksTheFirstThreeChannelsLittleEndian() {
    Manager manager;
    App::Settings settings{};
    FixedAdc adc;
    adc.values = {0x0123, 0x0456, 0x0789, 0x0ABC, 0x0DEF, 0x0FFF};
    const Frame reply = exchange(manager, settings, adc, Frame{readOf(0x00)});
    const Frame expected{0x00, 0x23, 0x01, 0x56, 0x04, 0x89, 0x07, 0x00};
    check(reply == expected, "channels 0 to 2 fill bytes 1 to 6, low byte first; channels 3 to 5 are not sent");
    check(adc.reads == 1, "a read of 0x00 reads the ADC once");
}

void writingTheAdcCommandOnlyEchoes() {
    Manager manager;
    App::Settings settings{};
    FixedAdc adc;
    adc.values = {0x0FFF, 0x0FFF, 0x0FFF, 0x0FFF, 0x0FFF, 0x0FFF};
    const Frame reply = exchange(manager, settings, adc, Frame{writeOf(0x00), 0xAA, 0xBB});
    check(reply == Frame{writeOf(0x00)}, "a write of 0x00 is echoed with zeros");
    check(adc.reads == 0, "a write of 0x00 does not read the ADC");
}

void eachWritableSettingRoundTrips() {
    struct Case {
        uint8_t command;
        uint16_t App::Settings::*field;
        const char *name;
    };
    const std::array<Case, 4> cases{{
        {0x01, &App::Settings::transparentThreshold, "0x01 is transparentThreshold"},
        {0x02, &App::Settings::nontransparentThreshold, "0x02 is nontransparentThreshold"},
        {0x03, &App::Settings::timeDelay, "0x03 is timeDelay"},
        {0x05, &App::Settings::deviceId, "0x05 is deviceId"},
    }};
    for (const Case &item : cases) {
        Manager manager;
        App::Settings settings{};
        FixedAdc adc;
        const Frame writeReply = exchange(manager, settings, adc, Frame{writeOf(item.command), 0x34, 0x12, 0x99});
        check(settings.*(item.field) == 0x1234, item.name);
        check(writeReply == Frame{writeOf(item.command)}, "a write is acknowledged by an echo with zeros");
        const Frame readReply = exchange(manager, settings, adc, Frame{readOf(item.command)});
        check(readReply == (Frame{readOf(item.command), 0x34, 0x12}), "a read returns the value low byte first");
    }
}

void aWriteChangesNoOtherSetting() {
    Manager manager;
    App::Settings settings{};
    FixedAdc adc;
    exchange(manager, settings, adc, Frame{writeOf(0x05), 0xFF, 0xFF});
    check(settings.deviceId == 0xFFFF, "deviceId takes all 16 bits");
    check(settings.transparentThreshold == 1000 && settings.nontransparentThreshold == 3000 &&
              settings.timeDelay == 100 && settings.filterId == 0 && settings.emptyValue == 500,
          "the other settings keep their defaults");
}

void sensorStatesSetOneBitPerChannelAboveTheNontransparentThreshold() {
    Manager manager;
    App::Settings settings{};
    FixedAdc adc;
    adc.values = {3001, 3000, 0, 4095, 2999, 3001};
    const Frame reply = exchange(manager, settings, adc, Frame{readOf(0x04)});
    check(reply == (Frame{readOf(0x04), 0x29}), "channels 0, 3 and 5 are above 3000: 0b101001");
    check(reply[1] == 0x29, "a reading equal to the threshold is not above it");
}

void sensorStatesCannotBeWritten() {
    Manager manager;
    App::Settings settings{};
    FixedAdc adc;
    adc.values = {4095, 4095, 4095, 4095, 4095, 4095};
    const Frame reply = exchange(manager, settings, adc, Frame{writeOf(0x04), 0x3F});
    check(reply == Frame{writeOf(0x04)}, "a write of 0x04 is echoed with zeros");
    check(adc.reads == 0, "a write of 0x04 does not read the ADC");
}

void unknownCommandsAreEchoedAndChangeNothing() {
    for (unsigned command = 0x06; command <= 0x7F; ++command) {
        Manager manager;
        App::Settings settings{};
        FixedAdc adc;
        const auto code = static_cast<uint8_t>(command);
        const Frame reply = exchange(manager, settings, adc, Frame{writeOf(code), 0x11, 0x22, 0x33});
        check(reply == Frame{writeOf(code)}, "an unknown command is echoed with zeros");
        check(settings.filterId == 0, "no command writes filterId, 0x06 included");
        check(adc.reads == 0, "an unknown command does not read the ADC");
    }
}

// AdcStub keeps one counter for every instance. Each test below makes 20 reads, a whole cycle, so each starts where
// the counter is a multiple of 20.
void theAdcStubTogglesEveryTenReads() {
    Hal::AdcStub adc;
    adc.init();
    static_assert(Hal::AdcStub::ChannelCount == 6, "six channels");
    for (int read = 1; read <= 20; ++read) {
        const auto &values = adc.readAll();
        const uint16_t expected = (read % 20 > 10) ? 3000 : 500;
        bool same = true;
        for (const uint16_t value : values) {
            same = same && value == expected;
        }
        check(same, "reads 1 to 10 and 20 give 500 on every channel, reads 11 to 19 give 3000");
    }
}

void withTheAdcStubAndTheDefaultsNoSensorIsEverFull() {
    Manager manager;
    App::Settings settings{};
    Hal::AdcStub adc;
    for (int read = 1; read <= 20; ++read) {
        const Frame reply = exchange(manager, settings, adc, Frame{readOf(0x04)});
        check(reply[1] == 0, "the stub's 3000 is not above the default threshold of 3000");
    }
}

void withALowerThresholdTheAdcStubFillsEverySensorForNineReads() {
    Manager manager;
    App::Settings settings{};
    Hal::AdcStub adc;
    settings.nontransparentThreshold = 2999;
    int full = 0;
    for (int read = 1; read <= 20; ++read) {
        const Frame reply = exchange(manager, settings, adc, Frame{readOf(0x04)});
        check(reply[1] == 0x00 || reply[1] == 0x3F, "all six sensors change together");
        full += reply[1] == 0x3F ? 1 : 0;
    }
    check(full == 9, "nine reads in twenty are full");
}

}  // namespace

int main() {
    theDefaultSettingsAreTheDocumentedOnes();
    isValidRejectsEachBrokenOrdering();
    initReachesTheTransport();
    nothingPendingMeansNothingSent();
    aFailedReceiveSendsNothing();
    everyReplyIsEightBytesAndEchoesByteZero();
    readingTheAdcPacksTheFirstThreeChannelsLittleEndian();
    writingTheAdcCommandOnlyEchoes();
    eachWritableSettingRoundTrips();
    aWriteChangesNoOtherSetting();
    sensorStatesSetOneBitPerChannelAboveTheNontransparentThreshold();
    sensorStatesCannotBeWritten();
    unknownCommandsAreEchoedAndChangeNothing();
    theAdcStubTogglesEveryTenReads();
    withTheAdcStubAndTheDefaultsNoSensorIsEverFull();
    withALowerThresholdTheAdcStubFillsEverySensorForNineReads();
    if (failures == 0) {
        std::printf("all checks passed\n");
    }
    return failures;
}
