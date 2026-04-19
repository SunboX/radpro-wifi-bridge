#include <cassert>
#include <iostream>

#include "Runtime/CooperativePump.h"

namespace
{
int callbackHits = 0;

void testCallback()
{
    ++callbackHits;
}

void testRegisteredCallbackRuns()
{
    callbackHits = 0;
    CooperativePump::setCallback(&testCallback);
    CooperativePump::service();
    assert(callbackHits == 1);
}

void testClearingCallbackStopsInvocations()
{
    callbackHits = 0;
    CooperativePump::setCallback(&testCallback);
    CooperativePump::clearCallback();
    CooperativePump::service();
    assert(callbackHits == 0);
}
} // namespace

int main()
{
    testRegisteredCallbackRuns();
    testClearingCallbackStopsInvocations();
    std::cout << "cooperative pump tests passed\n";
    return 0;
}
