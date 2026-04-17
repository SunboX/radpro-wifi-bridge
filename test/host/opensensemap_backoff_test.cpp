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
    testActiveBackoffIsPreservedByFreshReadings();
    testExpiredBackoffIsClearedByFreshReadings();
    testMissingBackoffStaysCleared();
    std::cout << "opensensemap backoff tests passed\n";
    return 0;
}
