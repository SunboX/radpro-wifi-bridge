// SPDX-FileCopyrightText: 2026 André Fiedler
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cassert>
#include <iostream>
#include "DeviceHealth/UsbRecoveryWatchdog.h"

namespace {
constexpr uint32_t fiveMinutes = 300000;
void testNeverRespondingDetectorDoesNotCauseBootLoops()
{
    UsbRecoveryWatchdog watchdog;
    assert(!watchdog.shouldRestart(600000, fiveMinutes, false));
    assert(!watchdog.armed());
}
void testLossDeadlineSurvivesHostRestartAttempts()
{
    UsbRecoveryWatchdog watchdog;
    watchdog.recordResponse(0);
    for (uint32_t now = 15000; now < fiveMinutes; now += 15000)
        assert(!watchdog.shouldRestart(now, fiveMinutes, false));
    assert(watchdog.shouldRestart(fiveMinutes, fiveMinutes, false));
    assert(watchdog.shouldRestart(fiveMinutes + 1, fiveMinutes, false));
}
void testRepliesResetTheDeadlineEvenWithoutPulseChanges()
{
    UsbRecoveryWatchdog watchdog;
    watchdog.recordResponse(1000);
    watchdog.recordResponse(290000);
    assert(!watchdog.shouldRestart(301000, fiveMinutes, false));
    assert(!watchdog.shouldRestart(589999, fiveMinutes, false));
    assert(watchdog.shouldRestart(590000, fiveMinutes, false));
}
void testUpdatesAndDisableDisarmUntilANewReply()
{
    UsbRecoveryWatchdog watchdog;
    watchdog.recordResponse(1000);
    assert(!watchdog.shouldRestart(500000, fiveMinutes, true));
    assert(!watchdog.armed());
    assert(!watchdog.shouldRestart(900000, fiveMinutes, false));
    watchdog.recordResponse(900000);
    assert(!watchdog.shouldRestart(1000000, 0, false));
    assert(!watchdog.shouldRestart(1500000, fiveMinutes, false));
    watchdog.recordResponse(1500000);
    assert(watchdog.shouldRestart(1800000, fiveMinutes, false));
}
void testTimerRollover()
{
    UsbRecoveryWatchdog watchdog;
    constexpr uint32_t start = UINT32_MAX - 20000;
    watchdog.recordResponse(start);
    assert(!watchdog.shouldRestart(static_cast<uint32_t>(start + fiveMinutes - 1), fiveMinutes, false));
    assert(watchdog.shouldRestart(static_cast<uint32_t>(start + fiveMinutes), fiveMinutes, false));
}
void testSlowPollingGetsAtLeastThreeIntervals()
{
    assert(UsbRecoveryWatchdog::effectiveTimeoutMs(300, 1000) == fiveMinutes);
    assert(UsbRecoveryWatchdog::effectiveTimeoutMs(60, 30000) == 90000);
    assert(UsbRecoveryWatchdog::effectiveTimeoutMs(0, 30000) == 0);
    assert(UsbRecoveryWatchdog::effectiveTimeoutMs(300, UINT32_MAX) == 0);
}
}
int main()
{
    testNeverRespondingDetectorDoesNotCauseBootLoops();
    testLossDeadlineSurvivesHostRestartAttempts();
    testRepliesResetTheDeadlineEvenWithoutPulseChanges();
    testUpdatesAndDisableDisarmUntilANewReply();
    testTimerRollover();
    testSlowPollingGetsAtLeastThreeIntervals();
    std::cout << "USB recovery watchdog tests passed\n";
}
