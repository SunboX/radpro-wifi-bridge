/*
 * SPDX-FileCopyrightText: 2026 André Fiedler
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include "AppConfig/AppConfig.h"
#include "DeviceInfo/DeviceInfoStore.h"
#include "DeviceManager.h"
#include "OpenRadiation/OpenRadiationMeasurementWindow.h"
#include "Publishing/PublisherHealth.h"

class OpenRadiationPublisher
{
public:
    OpenRadiationPublisher(AppConfig &config,
                           DeviceInfoStore &deviceInfo,
                           Print &log,
                           const char *bridgeVersion,
                           PublisherHealth &health);

    void begin();
    void updateConfig();
    void loop();
    void onCommandResult(DeviceManager::CommandType type, const String &value);
    void clearPendingData();

private:
    bool isEnabled() const;
    bool publishPending();
    bool sendPayload(const String &payload);
    bool buildPayload(String &outJson,
                      String &outReportUuid,
                      float doseRate,
                      const String &startTime,
                      const String &endTime,
                      const String &startPulseCount,
                      const String &endPulseCount,
                      String &outError);
    bool makeIsoTimestamp(String &out) const;
    String resolveApparatusId() const;
    String readResponseBody(WiFiClientSecure &client, unsigned long timeoutMs, size_t maxBytes) const;
    void syncHealthState();
    static String generateUuid();

    AppConfig &config_;
    DeviceInfoStore &deviceInfo_;
    Print &log_;
    String bridgeVersion_;
    PublisherHealth &health_;
    String pendingDoseValue_;
    String pendingTubeValue_;
    String lastPublishedReportUuid_;
    String lastConfigError_;
    OpenRadiationMeasurementWindow::MeasurementWindowState measurementWindow_;
    bool haveDoseValue_ = false;
    bool haveTubeValue_ = false;
    bool publishQueued_ = false;
    unsigned long lastAttemptMs_ = 0;
    unsigned long suppressUntilMs_ = 0;
};
