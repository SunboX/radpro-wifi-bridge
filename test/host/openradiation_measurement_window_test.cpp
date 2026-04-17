#include <cassert>
#include <iostream>
#include <string>

#include "OpenRadiation/OpenRadiationMeasurementWindow.h"

using OpenRadiationMeasurementWindow::MeasurementWindowState;
using OpenRadiationMeasurementWindow::armMeasurementWindow;
using OpenRadiationMeasurementWindow::clearMeasurementWindow;
using OpenRadiationMeasurementWindow::hasMeasurementWindow;
using OpenRadiationMeasurementWindow::replaceMeasurementWindow;

namespace
{
void testArmCapturesOnlyTheFirstQueuedWindow()
{
    MeasurementWindowState state;
    armMeasurementWindow(state, "2026-04-17T18:10:00Z", "100");
    armMeasurementWindow(state, "2026-04-17T18:11:00Z", "142");

    assert(std::string(state.startTime.c_str()) == "2026-04-17T18:10:00Z");
    assert(std::string(state.startPulseCount.c_str()) == "100");
}

void testClearResetsTheWindow()
{
    MeasurementWindowState state;
    armMeasurementWindow(state, "2026-04-17T18:10:00Z", "100");
    clearMeasurementWindow(state);

    assert(!hasMeasurementWindow(state));
    assert(std::string(state.startPulseCount.c_str()).empty());
}

void testEmptyTimestampDoesNotArmTheWindow()
{
    MeasurementWindowState state;
    armMeasurementWindow(state, "", "100");
    assert(!hasMeasurementWindow(state));
}

void testReplaceRefreshesQueuedMeasurementWindow()
{
    MeasurementWindowState state;
    armMeasurementWindow(state, "2026-04-17T18:10:00Z", "100");

    replaceMeasurementWindow(state, "2026-04-17T18:11:00Z", "142");

    assert(std::string(state.startTime.c_str()) == "2026-04-17T18:11:00Z");
    assert(std::string(state.startPulseCount.c_str()) == "142");
}

void testReplaceClearsStaleWindowWhenTimestampIsMissing()
{
    MeasurementWindowState state;
    armMeasurementWindow(state, "2026-04-17T18:10:00Z", "100");

    replaceMeasurementWindow(state, "", "142");

    assert(!hasMeasurementWindow(state));
    assert(std::string(state.startPulseCount.c_str()).empty());
}
} // namespace

int main()
{
    testArmCapturesOnlyTheFirstQueuedWindow();
    testClearResetsTheWindow();
    testEmptyTimestampDoesNotArmTheWindow();
    testReplaceRefreshesQueuedMeasurementWindow();
    testReplaceClearsStaleWindowWhenTimestampIsMissing();
    std::cout << "openradiation measurement window tests passed\n";
    return 0;
}
