#pragma once

#include <Arduino.h>

namespace OpenRadiationMeasurementWindow
{
struct MeasurementWindowState
{
    String startTime;
    String startPulseCount;
};

inline bool hasMeasurementWindow(const MeasurementWindowState &state)
{
    return state.startTime.length() > 0;
}

inline void armMeasurementWindow(MeasurementWindowState &state,
                                 const String &startTime,
                                 const String &startPulseCount)
{
    if (state.startTime.length() || !startTime.length())
        return;
    state.startTime = startTime;
    state.startPulseCount = startPulseCount;
}

inline void clearMeasurementWindow(MeasurementWindowState &state)
{
    state.startTime = String();
    state.startPulseCount = String();
}
} // namespace OpenRadiationMeasurementWindow
