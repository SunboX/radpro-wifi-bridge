// SPDX-FileCopyrightText: 2026 André Fiedler
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cassert>
#include <iostream>
#include "DeviceHealth/UsbRecoveryWatchdog.h"

namespace {
using Action = UsbRecoveryWatchdog::Action;
constexpr uint32_t fiveMinutes = 300000;
void testNeverRespondingDetectorDoesNotCauseBootLoops()
{
    UsbRecoveryWatchdog watchdog;
    assert(watchdog.check(600000, fiveMinutes, false, 0) == Action::None);
}
void testLossDeadlineSurvivesHostRestartAttempts()
{
    UsbRecoveryWatchdog watchdog;
    watchdog.recordResponse(0);
    for (uint32_t now = 15000; now < fiveMinutes; now += 15000)
        assert(watchdog.check(now, fiveMinutes, false, 0) == Action::None);
    assert(watchdog.check(fiveMinutes, fiveMinutes, false, 0) == Action::Restart);
    assert(watchdog.check(fiveMinutes + 1, fiveMinutes, false, 0) == Action::Restart);
}
void testRepliesResetTheDeadlineEvenWithoutPulseChanges()
{
    UsbRecoveryWatchdog watchdog;
    watchdog.recordResponse(1000);
    watchdog.recordResponse(290000);
    assert(watchdog.check(301000, fiveMinutes, false, 0) == Action::None);
    assert(watchdog.check(589999, fiveMinutes, false, 0) == Action::None);
    assert(watchdog.check(590000, fiveMinutes, false, 0) == Action::Restart);
}
void testUpdatesAndDisableDisarmUntilANewReply()
{
    UsbRecoveryWatchdog watchdog;
    watchdog.recordResponse(1000);
    assert(watchdog.check(500000, fiveMinutes, true, 0) == Action::None);
    assert(watchdog.check(900000, fiveMinutes, false, 0) == Action::None);
    watchdog.recordResponse(900000);
    assert(watchdog.check(1000000, 0, false, 0) == Action::None);
    assert(watchdog.check(1500000, fiveMinutes, false, 0) == Action::None);
    watchdog.recordResponse(1500000);
    assert(watchdog.check(1800000, fiveMinutes, false, 0) == Action::Restart);
}
void testReappearingDetectorCanRecoverAfterAnOverdueCheck()
{
    UsbRecoveryWatchdog watchdog;
    watchdog.recordResponse(1000);
    constexpr uint32_t resumed = 860000;
    const uint32_t readyAt = UsbAttachDelayPolicy::readyAt(resumed);
    assert(watchdog.check(resumed, fiveMinutes, false, readyAt) == Action::WaitForTelemetry);
    assert(watchdog.check(resumed + 5400, fiveMinutes, false, readyAt) == Action::None);
    assert(!watchdog.recordResponse(resumed + 6000));
    // The main task sampled its clock just before the USB task recorded that reply.
    assert(watchdog.check(resumed + 5999, fiveMinutes, false, readyAt) == Action::None);
    assert(watchdog.check(resumed + 6000 + fiveMinutes, fiveMinutes, false, readyAt) == Action::Restart);
}
void testRepeatedAttachmentsCannotKeepASilentDetectorAlive()
{
    UsbRecoveryWatchdog watchdog;
    constexpr uint32_t firstCheck = UINT32_MAX - 10000;
    watchdog.recordResponse(firstCheck - fiveMinutes);
    assert(watchdog.check(firstCheck, fiveMinutes, false, firstCheck + 250) == Action::WaitForTelemetry);
    for (uint32_t elapsed = 1000; elapsed < UsbAttachDelayPolicy::kOpeningGraceMs; elapsed += 1000)
    {
        uint32_t now = firstCheck + elapsed;
        assert(watchdog.check(now, fiveMinutes, false, now + 250) == Action::None);
    }
    const uint32_t expired = firstCheck + UsbAttachDelayPolicy::kOpeningGraceMs;
    assert(watchdog.check(expired, fiveMinutes, false, expired + 250) == Action::Restart);
    assert(watchdog.check(expired + 1, fiveMinutes, true, expired + 250) == Action::None);
    assert(watchdog.recordResponse(expired + 100));
}
void testTimerRollover()
{
    UsbRecoveryWatchdog watchdog;
    constexpr uint32_t start = UINT32_MAX - 20000;
    watchdog.recordResponse(start);
    assert(watchdog.check(static_cast<uint32_t>(start + fiveMinutes - 1), fiveMinutes, false, 0) == Action::None);
    assert(watchdog.check(static_cast<uint32_t>(start + fiveMinutes), fiveMinutes, false, 0) == Action::Restart);
}
void testSlowPollingGetsAtLeastThreeIntervals()
{
    assert(UsbRecoveryWatchdog::effectiveTimeoutMs(300, 1000) == fiveMinutes);
    assert(UsbRecoveryWatchdog::effectiveTimeoutMs(60, 30000) == 90000);
    assert(UsbRecoveryWatchdog::effectiveTimeoutMs(0, 30000) == 0);
    assert(UsbRecoveryWatchdog::effectiveTimeoutMs(300, UINT32_MAX) == 0);
}
void testBlockedPollingGetsOneRealRetry()
{
    UsbRecoveryWatchdog watchdog;
    const uint32_t start = UINT32_MAX - 500;
    watchdog.recordResponse(start);
    watchdog.recordPollService(start);
    const uint32_t resumed = start + fiveMinutes + 1000;
    // A long network operation can finish on either side of the detector service in loop().
    assert(watchdog.check(resumed, fiveMinutes, false, 0, true) == Action::RetryPoll);
    watchdog.recordPollService(resumed);
    assert(watchdog.check(resumed + 5, fiveMinutes, false, 0, true) == Action::None);
    assert(watchdog.check(resumed + UsbAttachDelayPolicy::kOpeningGraceMs, fiveMinutes, false, 0, true) == Action::Restart);

    watchdog.reset();
    watchdog.recordResponse(start);
    watchdog.recordPollService(start);
    watchdog.recordPollService(resumed);
    assert(watchdog.check(resumed, fiveMinutes, false, 0, true) == Action::RetryPoll);
    watchdog.recordResponse(resumed + 25);
    assert(watchdog.check(resumed + 30, fiveMinutes, false, 0, true) == Action::None);
}
void testSilentAndDisconnectedDetectorsStillRestart()
{
    UsbRecoveryWatchdog watchdog;
    watchdog.recordResponse(0);
    for (uint32_t t = 0; t < fiveMinutes; t += 1000)
    {
        watchdog.recordPollService(t);
        assert(watchdog.check(t, fiveMinutes, false, 0, true) == Action::None);
    }
    assert(watchdog.check(fiveMinutes, fiveMinutes, false, 0, true) == Action::Restart);
    watchdog.reset();
    watchdog.recordResponse(0);
    watchdog.recordPollService(0);
    assert(watchdog.check(fiveMinutes, fiveMinutes, false, 0, false) == Action::Restart);
}
}
int main()
{
    testNeverRespondingDetectorDoesNotCauseBootLoops();
    testLossDeadlineSurvivesHostRestartAttempts();
    testRepliesResetTheDeadlineEvenWithoutPulseChanges();
    testUpdatesAndDisableDisarmUntilANewReply();
    testReappearingDetectorCanRecoverAfterAnOverdueCheck();
    testRepeatedAttachmentsCannotKeepASilentDetectorAlive();
    testTimerRollover();
    testSlowPollingGetsAtLeastThreeIntervals();
    testBlockedPollingGetsOneRealRetry();
    testSilentAndDisconnectedDetectorsStillRestart();
    std::cout << "USB recovery watchdog tests passed\n";
}
