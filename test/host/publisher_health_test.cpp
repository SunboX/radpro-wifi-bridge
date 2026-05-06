// SPDX-FileCopyrightText: 2026 André Fiedler
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cassert>
#include <iostream>
#include <string>

#include "Publishing/PublisherHealth.h"

namespace
{
void testFailureTrackingCapturesErrorAndTrace()
{
    PublisherHealth health;
    health.setEnabled(true);
    health.setPaused(false);
    health.setPending(true);

    health.noteAttempt(1000);
    health.noteFailure(1100, "no response", 0, String(), "raw trace");

    const auto snapshot = health.snapshot();
    assert(snapshot.enabled);
    assert(!snapshot.paused);
    assert(snapshot.pending);
    assert(snapshot.attempts == 1);
    assert(snapshot.successes == 0);
    assert(snapshot.failures == 1);
    assert(snapshot.consecutiveFailures == 1);
    assert(snapshot.lastAttemptMs == 1000);
    assert(snapshot.lastFailureMs == 1100);
    assert(snapshot.lastStatusCode == 0);
    assert(std::string(snapshot.lastError.c_str()) == "no response");
    assert(std::string(snapshot.lastResponseTrace.c_str()) == "raw trace");
}

void testSuccessResetsConsecutiveFailures()
{
    PublisherHealth health;
    health.noteAttempt(1000);
    health.noteFailure(1010, "first failure");
    health.noteAttempt(2000);
    health.noteSuccess(2020, 204, "HTTP/1.1 204 No Content");

    const auto snapshot = health.snapshot();
    assert(snapshot.attempts == 2);
    assert(snapshot.successes == 1);
    assert(snapshot.failures == 1);
    assert(snapshot.consecutiveFailures == 0);
    assert(snapshot.lastSuccessMs == 2020);
    assert(snapshot.lastStatusCode == 204);
    assert(std::string(snapshot.lastStatusLine.c_str()) == "HTTP/1.1 204 No Content");
    assert(std::string(snapshot.lastError.c_str()).empty());
}

void testTracksLastReportUuidSeparatelyFromSuccessCounters()
{
    PublisherHealth health;
    health.setLastReportUuid("report-123");
    health.noteAttempt(3000);
    health.noteSuccess(3010, 200, "HTTP/1.1 200 OK");

    const auto snapshot = health.snapshot();
    assert(std::string(snapshot.lastReportUuid.c_str()) == "report-123");
    assert(snapshot.successes == 1);

    health.noteFailure(4000, "temporary upstream error");
    assert(std::string(health.snapshot().lastReportUuid.c_str()) == "report-123");
}
} // namespace

int main()
{
    testFailureTrackingCapturesErrorAndTrace();
    testSuccessResetsConsecutiveFailures();
    testTracksLastReportUuidSeparatelyFromSuccessCounters();
    std::cout << "publisher health tests passed\n";
    return 0;
}
