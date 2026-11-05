// SPDX-FileCopyrightText: 2026 André Fiedler
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cassert>
#include <iostream>
#include <sys/time.h>
#include "esp_timer.h"
#include "Logging/DebugLogStream.h"

namespace
{
timeval wallTime{};
int clockResult = 0;
constexpr const char *kUtcPrefix = "[12:34:56.123 UTC] ";

void setClock(time_t seconds, long microseconds)
{
    wallTime.tv_sec = seconds;
    wallTime.tv_usec = microseconds;
    clockResult = 0;
}

void testFragmentedWritesShareOneTimestampOnBothOutputs()
{
    // 2024-01-01 12:34:56.123 UTC.
    setClock(1704112496, 123456);
    HardwareSerial serial;
    DebugLogStream log(serial);
    assert(log.print("Tube ") == 5);
    setClock(1704112497, 987654);
    assert(log.print("Rate: ") == 6);
    assert(log.println("27") == 3);
    assert(serial.output == std::string(kUtcPrefix) + "Tube Rate: 27\n");
    std::vector<DebugLogEntry> entries;
    log.copyEntries(entries);
    assert(entries.size() == 1);
    assert(entries[0].text == String(std::string(kUtcPrefix) + "Tube Rate: 27"));
}

void testMultilineAndCrLfWrites()
{
    setClock(1704112496, 123456);
    HardwareSerial serial;
    DebugLogStream log(serial);
    const std::string message = "First\r\n\nThird\n";
    assert(log.write(reinterpret_cast<const uint8_t *>(message.data()), message.size()) == message.size());
    assert(serial.output == std::string(kUtcPrefix) + "First\r\n" + kUtcPrefix + "\n" + kUtcPrefix + "Third\n");
    std::vector<DebugLogEntry> entries;
    log.copyEntries(entries);
    assert(entries.size() == 3);
    assert(entries[0].text == String(std::string(kUtcPrefix) + "First"));
    assert(entries[1].text == String(kUtcPrefix));
    assert(entries[2].text == String(std::string(kUtcPrefix) + "Third"));
}

void testUptimeBeforeSyncAndUtcAfterSync()
{
    setClock(0, 0);
    // Longer than millis()'s 32-bit rollover, without losing elapsed days.
    g_test_uptime_us = 4294967296LL * 1000 + 123000;
    HardwareSerial serial;
    DebugLogStream log(serial);
    log.println("Starting");
    setClock(1704112496, 123456);
    log.println("Synced");
    assert(serial.output == std::string("[up 1193:02:47.419] Starting\n") + kUtcPrefix + "Synced\n");
    std::vector<DebugLogEntry> entries;
    log.copyEntries(entries);
    assert(entries.size() == 2);
    assert(entries[0].text == "[up 1193:02:47.419] Starting");
    assert(entries[1].text == String(std::string(kUtcPrefix) + "Synced"));
}

void testClockFailureUsesUptime()
{
    setClock(1704112496, 123456);
    clockResult = -1;
    g_test_uptime_us = 5123000;
    HardwareSerial serial;
    DebugLogStream log(serial);
    log.println("No clock");
    assert(serial.output == "[up 00:00:05.123] No clock\n");
}

void testLongLinesAndPartialLinesKeepTheirOriginalTimestamp()
{
    setClock(1704112496, 123456);
    HardwareSerial serial;
    DebugLogStream log(serial);
    const std::string payload(700, 'x');
    assert(log.print(payload.c_str()) == payload.size());
    std::vector<DebugLogEntry> entries;
    log.copyEntries(entries);
    assert(entries.size() == 3);
    std::string rebuilt;
    for (const auto &entry : entries)
    {
        const std::string text(entry.text.c_str());
        assert(text.find(kUtcPrefix) == 0);
        assert(text.size() <= 320);
        rebuilt += text.substr(std::strlen(kUtcPrefix));
    }
    assert(rebuilt == payload);
    assert(serial.output == std::string(kUtcPrefix) + payload);
    setClock(1704112497, 0);
    log.println();
    log.println("Next");
    log.copyEntries(entries);
    assert(entries.size() == 4);
    assert(entries.back().text == "[12:34:57.000 UTC] Next");
}

void testEmptyWritesAndRingBufferBound()
{
    setClock(1704112496, 123456);
    HardwareSerial serial;
    DebugLogStream log(serial, 2);
    assert(log.write(nullptr, 0) == 0);
    assert(serial.output.empty());
    log.println("First");
    log.println("Second");
    log.println("Third");
    std::vector<DebugLogEntry> entries;
    log.copyEntries(entries);
    assert(entries.size() == 2);
    assert(entries.front().id == 2 && entries.back().id == 3);
    assert(log.latestId() == 3);
}
}

extern "C" int radpro_test_gettimeofday(timeval *now, void *)
{
    *now = wallTime;
    return clockResult;
}

int main()
{
    testFragmentedWritesShareOneTimestampOnBothOutputs();
    testMultilineAndCrLfWrites();
    testUptimeBeforeSyncAndUtcAfterSync();
    testClockFailureUsesUptime();
    testLongLinesAndPartialLinesKeepTheirOriginalTimestamp();
    testEmptyWritesAndRingBufferBound();
    std::cout << "debug log stream tests passed\n";
}
