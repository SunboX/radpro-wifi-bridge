// SPDX-License-Identifier: GPL-3.0-or-later
#include "Runtime/StallHistory.h"
#include <cassert>
#include <cstring>
#include <type_traits>

using namespace StallRecorder;
int main()
{
    static_assert(std::is_trivially_copyable<History>::value, "NVS stores a fixed-size record");
    Monitor m;
    Sample s{};
    s.havePoll = s.haveReply = true;
    s.uptimeMs = 100000;
    s.stage = Stage::OpenSenseMap;
    s.stageMs = s.replyMs = s.pollMs = Monitor::thresholdMs;
    assert(!m.observe(1, 5, s, false, 1000));
    assert(!m.observe(1, 5, s, true, 60000));
    assert(m.observe(1, 5, s, true, 1000));
    for (int i = 0; i < 100; ++i) assert(!m.observe(1, 6, s, true, 1000));
    assert(m.history.count == 1);
    s.stage = Stage::Recovery;
    assert(m.observe(1, 7, s, true, 1000, true));
    const Incident &incident = m.history.records[m.history.newest()];
    assert(incident.outcome == Outcome::UsbWatchdog && incident.first.stage == Stage::OpenSenseMap);
    assert(incident.last.stage == Stage::Recovery);
    assert(!m.observe(1, 7, s, true, 1000)); // No second incident while the reset is pending.

    Monitor nextBoot;
    std::memcpy(&nextBoot.history, &m.history, sizeof(History));
    s.stageMs = s.pollMs = 1;
    assert(nextBoot.observe(2, 8, s, true, 1000)); // Polling continued, USB replies stopped.
    s.replyMs = 5;
    assert(nextBoot.observe(2, 9, s, true, 1000));
    assert(nextBoot.history.records[nextBoot.history.newest()].outcome == Outcome::Recovered);
    assert(!nextBoot.observe(2, 10, s, true, 1000));
    for (int i = 0; i < 5; ++i)
    {
        s.replyMs = Monitor::thresholdMs;
        assert(nextBoot.observe(2, 11+i, s, true, 1000));
        assert(nextBoot.observe(2, 12+i, s, false, 1000));
    }
    assert(nextBoot.history.valid() && nextBoot.history.count == History::capacity);
    Monitor powerCycle;
    s.replyMs = Monitor::thresholdMs;
    assert(nextBoot.observe(2, 15, s, true, 1000));
    powerCycle.history = nextBoot.history;
    s.replyMs = 1;
    assert(!powerCycle.observe(3, 20, s, true, 1000));
    assert(powerCycle.history.records[powerCycle.history.newest()].bootId == 2);
    assert(powerCycle.observe(3, 21, s, true, 1000, true)); // Reboot before first sampler tick.
    assert(powerCycle.history.records[powerCycle.history.newest()].outcome == Outcome::UsbWatchdog);

    Monitor slow;
    s.pollMs = s.replyMs = s.stageMs = 60000;
    assert(slow.observe(4, 1, s, true, 60000));
    s.uptimeMs += 1000;
    s.replyMs += 1000;
    s.pollMs = s.stageMs = 5;
    assert(!slow.observe(4, 2, s, true, 60000)); // The app resumed and polled, but no reply yet.
    s.replyMs = 3;
    assert(slow.observe(4, 3, s, true, 60000));
    assert(slow.history.records[slow.history.newest()].outcome == Outcome::Recovered);
}
