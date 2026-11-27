// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "Runtime/StallHistory.h"
#include <ArduinoJson.h>

class UsbCdcHost;
class DeviceManager;

namespace StallRecorder
{
bool begin(UsbCdcHost &host, DeviceManager &device, uint32_t bootId);
struct Location { Stage stage; uint32_t since; };
Location enter(Stage stage);
void restore(Location location);
class ScopedStage
{
public:
    explicit ScopedStage(Stage stage) : previous_(enter(stage)) {}
    ~ScopedStage() { restore(previous_); }
    ScopedStage(const ScopedStage &) = delete;
    ScopedStage &operator=(const ScopedStage &) = delete;
private:
    Location previous_;
};
void setPolling(bool enabled, uint32_t intervalMs, bool wifiConnected);
void requestedPoll();
void receivedTelemetry();
void wifiDisconnected(uint8_t reason);
void mqttServiceFinished(int clientState);
void mqttWriteFailed(uint32_t elapsedMs, int error);
void beforeWatchdogRestart();
void appendJson(JsonObject out);
}
