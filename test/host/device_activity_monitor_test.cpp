#include <cassert>
#include <iostream>

#include "DeviceHealth/DeviceActivityMonitor.h"

namespace
{
void testPowerOffTriggersTelemetryFault()
{
    DeviceActivityMonitor monitor;

    monitor.onCommandResult(DeviceManager::CommandType::DevicePower, "0", true, 1000);

    assert(monitor.fault() == DeviceActivityFault::PowerOff);
    assert(monitor.hasFault());
    assert(monitor.shouldSuppressTelemetry(DeviceManager::CommandType::TubePulseCount));
    assert(monitor.shouldSuppressTelemetry(DeviceManager::CommandType::TubeRate));
    assert(monitor.shouldSuppressTelemetry(DeviceManager::CommandType::TubeDoseRate));
    assert(!monitor.shouldSuppressTelemetry(DeviceManager::CommandType::DevicePower));
}

void testPowerOnClearsPowerFault()
{
    DeviceActivityMonitor monitor;

    monitor.onCommandResult(DeviceManager::CommandType::DevicePower, "0", true, 1000);
    monitor.onCommandResult(DeviceManager::CommandType::DevicePower, "1", true, 1500);

    assert(monitor.fault() == DeviceActivityFault::None);
    assert(!monitor.hasFault());
}

void testStalePulseCountTriggersAfterTimeout()
{
    DeviceActivityMonitor monitor;
    monitor.setStalePulseTimeoutMs(5000);

    monitor.onCommandResult(DeviceManager::CommandType::TubePulseCount, "347925", true, 1000);
    monitor.evaluate(5999);

    assert(monitor.fault() == DeviceActivityFault::None);

    monitor.onCommandResult(DeviceManager::CommandType::TubePulseCount, "347925", true, 6000);

    assert(monitor.fault() == DeviceActivityFault::StalePulseCount);
    assert(monitor.hasFault());
    assert(monitor.shouldSuppressTelemetry(DeviceManager::CommandType::TubeRate));
}

void testPulseCountAdvanceClearsStaleFault()
{
    DeviceActivityMonitor monitor;
    monitor.setStalePulseTimeoutMs(5000);

    monitor.onCommandResult(DeviceManager::CommandType::TubePulseCount, "347925", true, 1000);
    monitor.onCommandResult(DeviceManager::CommandType::TubePulseCount, "347925", true, 6000);
    monitor.onCommandResult(DeviceManager::CommandType::TubePulseCount, "347926", true, 6500);

    assert(monitor.fault() == DeviceActivityFault::None);
    assert(!monitor.hasFault());
}

void testRepeatedTelemetryFailuresTriggerFaultAfterKnownTelemetry()
{
    DeviceActivityMonitor monitor;
    monitor.setMissingTelemetryTimeoutMs(5000);

    monitor.onCommandResult(DeviceManager::CommandType::DevicePower, "1", true, 1000);
    monitor.onCommandResult(DeviceManager::CommandType::TubePulseCount, "347929", true, 1200);
    monitor.onCommandResult(DeviceManager::CommandType::DevicePower, "", false, 6100);

    assert(monitor.fault() == DeviceActivityFault::None);

    monitor.onCommandResult(DeviceManager::CommandType::TubePulseCount, "", false, 6200);

    assert(monitor.fault() == DeviceActivityFault::TelemetryTimeout);
    assert(monitor.hasFault());
    assert(monitor.shouldSuppressTelemetry(DeviceManager::CommandType::TubeRate));
}

void testTelemetrySuccessClearsTelemetryTimeoutFault()
{
    DeviceActivityMonitor monitor;
    monitor.setMissingTelemetryTimeoutMs(5000);

    monitor.onCommandResult(DeviceManager::CommandType::DevicePower, "1", true, 1000);
    monitor.onCommandResult(DeviceManager::CommandType::TubePulseCount, "347929", true, 1200);
    monitor.onCommandResult(DeviceManager::CommandType::DevicePower, "", false, 6100);
    monitor.onCommandResult(DeviceManager::CommandType::TubePulseCount, "", false, 6200);
    monitor.onCommandResult(DeviceManager::CommandType::DevicePower, "1", true, 6500);

    assert(monitor.fault() == DeviceActivityFault::None);
    assert(!monitor.hasFault());
}
} // namespace

int main()
{
    testPowerOffTriggersTelemetryFault();
    testPowerOnClearsPowerFault();
    testStalePulseCountTriggersAfterTimeout();
    testPulseCountAdvanceClearsStaleFault();
    testRepeatedTelemetryFailuresTriggerFaultAfterKnownTelemetry();
    testTelemetrySuccessClearsTelemetryTimeoutFault();
    std::cout << "device activity monitor tests passed\n";
    return 0;
}
