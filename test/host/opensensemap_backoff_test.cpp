// SPDX-FileCopyrightText: 2026 André Fiedler
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cassert>
#include <iostream>

#include "OpenSenseMap/OpenSenseMapBackoff.h"

static void testActiveBackoffIsPreservedByFreshReadings()
{
    const unsigned long now = 12'000;
    const unsigned long suppressUntil = 72'000;

    const unsigned long preserved =
        OpenSenseMapBackoff::preserveActiveSuppression(now, suppressUntil);

    assert(preserved == suppressUntil);
}

static void testExpiredBackoffIsClearedByFreshReadings()
{
    const unsigned long now = 90'000;
    const unsigned long suppressUntil = 72'000;

    const unsigned long cleared =
        OpenSenseMapBackoff::preserveActiveSuppression(now, suppressUntil);

    assert(cleared == 0);
}

static void testMissingBackoffStaysCleared()
{
    const unsigned long now = 12'000;

    const unsigned long cleared =
        OpenSenseMapBackoff::preserveActiveSuppression(now, 0);

    assert(cleared == 0);
}

int main()
{
    assert(OpenSenseMapBackoff::kMinPublishGapMs == 15000);
    assert(OpenSenseMapBackoff::retryDelayMs(1, 0) == 10000);
    assert(OpenSenseMapBackoff::retryDelayMs(2, 503) == 20000);
    assert(OpenSenseMapBackoff::retryDelayMs(3, 503) == 40000);
    assert(OpenSenseMapBackoff::retryDelayMs(8, 503) == 60000);
    const auto rateLimitedUntil = 12000 + OpenSenseMapBackoff::retryDelayMs(1, 429);
    assert(rateLimitedUntil == 72000);
    assert(OpenSenseMapBackoff::preserveActiveSuppression(13000, rateLimitedUntil) == rateLimitedUntil);
    assert(OpenSenseMapBackoff::preserveActiveSuppression(72000, rateLimitedUntil) == 0);
    testActiveBackoffIsPreservedByFreshReadings();
    testExpiredBackoffIsClearedByFreshReadings();
    testMissingBackoffStaysCleared();
    std::cout << "opensensemap backoff tests passed\n";
    return 0;
}
