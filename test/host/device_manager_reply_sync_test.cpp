// SPDX-FileCopyrightText: 2026 André Fiedler
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cassert>
#include <iostream>
#include <vector>
#include "DeviceManager.h"

namespace {
using Type = DeviceManager::CommandType;
constexpr const char *identity = "OK FNIRSI GC-03;Rad Pro 3.1.1/de;gc03-test";
struct Value { Type type; String value; bool success; };
struct Fixture {
    UsbCdcHost host;
    DeviceManager manager{host};
    std::vector<Value> values;
    Fixture() {
        setMillis(0);
        manager.setCommandResultHandler([this](Type type, const String &value, bool success) {
            values.push_back({type, value, success});
        });
        manager.begin(std::vector<std::pair<uint16_t,uint16_t>>{});
        manager.start(); host.simulateConnect(); advanceMillis(100); manager.loop();
    }
    void metadata() {
        for (const char *reply : {"OK 1", "OK 4.377", "OK 1793977000", "OK 1.0",
                                  "OK 16000", "OK 108.345", "OK 0.0002420", "OK 0.0002500"})
            host.simulateLine(reply);
    }
    void initialize() { host.simulateLine(identity); metadata(); values.clear(); }
    void expectCommand(const char *command) { assert(host.sentCommands().back() == command); }
};
void testInitialIdentityRejectsNumericAndEmptyReplies() {
    Fixture f; const auto count = f.host.sentCommands().size();
    for (const char *reply : {"OK 1", "OK 33675", "OK 4.377", "OK model;", "OK model;firmware; "})
        f.host.simulateLine(reply);
    assert(f.host.sentCommands().size() == count && f.values.empty());
    f.host.simulateLine(identity); f.expectCommand("GET devicePower\r\n");
}
void testDelayedRepliesCannotShiftTheTelemetryFields() {
    Fixture f; f.initialize(); f.manager.requestStats(); f.expectCommand("GET devicePower\r\n");
    const auto sent = f.host.sentCommands().size();
    advanceMillis(12001); f.manager.loop();
    assert(f.host.sentCommands().size() == sent);
    for (int i=0; i<100; ++i) f.manager.requestStats();
    f.manager.requestRandomData();
    f.manager.requestDataLog("0 10");
    f.host.simulateLine("OK 1"); // expired power response during the quiet period
    advanceMillis(250); f.manager.loop();
    assert(f.host.sentCommands().size() == sent + 1); // one identity probe, no numeric query
    f.expectCommand("GET deviceId\r\n");
    const auto accepted = f.values.size();
    for (const char *reply : {"OK 1", "OK 33675", "OK 19.227", "OK 4.377", "ERROR",
                             "OK FNIRSI GC-03;Rad Pro 3.1.1/de;another-detector"})
        f.host.simulateLine(reply);
    assert(f.values.size() == accepted);
    f.host.simulateLine(identity); f.expectCommand("GET devicePower\r\n"); f.metadata();
    f.values.clear(); f.manager.requestStats();
    for (const char *reply : {"OK 1", "OK 33675", "OK 19.227", "OK 4.377"})
        f.host.simulateLine(reply);
    bool rate=false, battery=false;
    for (const auto &v : f.values) {
        if (v.type == Type::TubeRate) { assert(v.value == "19.227"); rate=true; }
        if (v.type == Type::DeviceBatteryVoltage) { assert(v.value == "4.377"); battery=true; }
        if (v.type == Type::TubeDoseRate) assert(v.value.toFloat() < 1.0f);
    }
    assert(rate && battery);
}
void testRateTimeoutDoesNotConsumeAnOldRateAsBatteryVoltage() {
    Fixture f; f.initialize(); f.manager.requestStats();
    f.host.simulateLine("OK 1"); f.host.simulateLine("OK 33675"); f.expectCommand("GET tubeRate\r\n");
    f.values.clear(); advanceMillis(12001); f.manager.loop(); advanceMillis(250); f.manager.loop();
    f.expectCommand("GET deviceId\r\n"); f.host.simulateLine("OK 19.227");
    for (const auto &v : f.values) assert(!v.success);
    advanceMillis(12001); f.manager.loop(); assert(f.host.restarted());
}
void testGenuineHighRateIsPreserved() {
    Fixture f; f.initialize(); f.manager.requestStats();
    for (const char *reply : {"OK 1", "OK 33675", "OK 33675", "OK 4.377"}) f.host.simulateLine(reply);
    bool high=false;
    for (const auto &v : f.values)
        if (v.type == Type::TubeDoseRate) { assert(v.value.toFloat() > 310.0f); high=true; }
    assert(high);
}
}
int main() {
    testInitialIdentityRejectsNumericAndEmptyReplies();
    testDelayedRepliesCannotShiftTheTelemetryFields();
    testRateTimeoutDoesNotConsumeAnOldRateAsBatteryVoltage();
    testGenuineHighRateIsPreserved();
    std::cout << "USB reply synchronization tests passed\n";
}
