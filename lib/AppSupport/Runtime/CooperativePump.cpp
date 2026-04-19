#include "Runtime/CooperativePump.h"

namespace
{
CooperativePump::Callback g_callback = nullptr;
}

void CooperativePump::setCallback(Callback callback)
{
    g_callback = callback;
}

void CooperativePump::clearCallback()
{
    g_callback = nullptr;
}

void CooperativePump::service()
{
    if (g_callback)
    {
        g_callback();
        return;
    }

    delay(10);
    yield();
}
