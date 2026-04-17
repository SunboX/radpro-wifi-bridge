#include <cassert>
#include <iostream>
#include <vector>

#include "OpenSenseMap/OpenSenseMapTls.h"

namespace
{
struct FakeClient
{
    std::vector<int> availableSequence;
    std::vector<bool> connectedSequence;
    size_t availableIndex = 0;
    size_t connectedIndex = 0;

    int available()
    {
        if (availableIndex < availableSequence.size())
            return availableSequence[availableIndex++];
        return availableSequence.empty() ? 0 : availableSequence.back();
    }

    bool connected()
    {
        if (connectedIndex < connectedSequence.size())
            return connectedSequence[connectedIndex++];
        return connectedSequence.empty() ? true : connectedSequence.back();
    }
};

void testPositiveCodesAreNotTreatedAsTlsErrors()
{
    assert(OpenSenseMapTls::normalizeMbedTlsErrorCode(56) == 0);
    assert(OpenSenseMapTls::normalizeMbedTlsErrorCode(-56) == -56);
}

void testCtrDrbgDetectionOnlyMatchesRealMbedTlsError()
{
    assert(OpenSenseMapTls::isCtrDrbgInputTooLarge(-0x0038));
    assert(!OpenSenseMapTls::isCtrDrbgInputTooLarge(56));
    assert(!OpenSenseMapTls::isCtrDrbgInputTooLarge(0));
}

void testWaitForResponseReturnsTrueWhenDataArrives()
{
    FakeClient client{{0, 0, 12}, {true, true, true}};
    unsigned long now = 0;
    const bool ok = OpenSenseMapTls::waitForResponse(
        client,
        100,
        [&now]() { return now; },
        [&now]() { now += 10; });
    assert(ok);
}

void testWaitForResponseIgnoresEarlyDisconnectStateWhilePolling()
{
    FakeClient client{{0, 0, 8}, {false, false, false}};
    unsigned long now = 0;
    const bool ok = OpenSenseMapTls::waitForResponse(
        client,
        100,
        [&now]() { return now; },
        [&now]() { now += 10; });
    assert(ok);
}

void testWaitForResponseReturnsFalseOnTimeout()
{
    FakeClient client{{0, 0, 0, 0}, {true, true, true, true}};
    unsigned long now = 0;
    const bool ok = OpenSenseMapTls::waitForResponse(
        client,
        25,
        [&now]() { return now; },
        [&now]() { now += 10; });
    assert(!ok);
}

void testWaitForResponseStopsOnReadError()
{
    FakeClient client{{-1}, {true}};
    unsigned long now = 0;
    const bool ok = OpenSenseMapTls::waitForResponse(
        client,
        100,
        [&now]() { return now; },
        [&now]() { now += 10; });
    assert(!ok);
}
} // namespace

int main()
{
    testPositiveCodesAreNotTreatedAsTlsErrors();
    testCtrDrbgDetectionOnlyMatchesRealMbedTlsError();
    testWaitForResponseReturnsTrueWhenDataArrives();
    testWaitForResponseIgnoresEarlyDisconnectStateWhilePolling();
    testWaitForResponseReturnsFalseOnTimeout();
    testWaitForResponseStopsOnReadError();
    std::cout << "opensensemap tls tests passed\n";
    return 0;
}
