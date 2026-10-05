// SPDX-FileCopyrightText: 2026 André Fiedler
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "DeviceInfoPage.h"
#include <LittleFS.h>

namespace
{
void appendPrometheusLabelValue(String &metrics, const String &value)
{
    for (size_t i = 0; i < value.length(); ++i)
    {
        switch (value[i])
        {
        case '\\':
            metrics += F("\\\\");
            break;
        case '"':
            metrics += F("\\\"");
            break;
        case '\n':
            metrics += F("\\n");
            break;
        default:
            metrics += value[i];
            break;
        }
    }
}
} // namespace

DeviceInfoPage::DeviceInfoPage(DeviceInfoStore &store) : store_(store) {}

void DeviceInfoPage::handlePage(WiFiManager *manager)
{
    if (!manager || !manager->server)
        return;

    File file = LittleFS.open("/portal/device-info.html", "r");
    if (!file)
    {
        manager->server->send(500, "text/plain", "Device info page missing.");
        return;
    }

    String html = file.readString();
    file.close();
    manager->server->send(200, "text/html", html);
}

void DeviceInfoPage::handleJson(WiFiManager *manager)
{
    if (!manager || !manager->server)
        return;
    manager->server->send(200, "application/json", store_.toJson());
}

void DeviceInfoPage::handlePrometheus(WiFiManager *manager)
{
    if (!manager || !manager->server)
        return;

    DeviceInfoSnapshot snap = store_.snapshot();
    String metrics;

    metrics += F("# HELP radpro_info A metric with constant 1 value labeled by device info.\n");
    metrics += F("# TYPE radpro_info gauge\n");
    metrics += F("radpro_info{manufacturer=\"");
    appendPrometheusLabelValue(metrics, snap.manufacturer);
    metrics += F("\",model=\"");
    appendPrometheusLabelValue(metrics, snap.model);
    metrics += F("\",firmware=\"");
    appendPrometheusLabelValue(metrics, snap.firmware);
    metrics += F("\",bridge_firmware=\"");
    appendPrometheusLabelValue(metrics, snap.bridgeFirmware);
    metrics += F("\",device_id=\"");
    appendPrometheusLabelValue(metrics, snap.deviceId);
    metrics += F("\",locale=\"");
    appendPrometheusLabelValue(metrics, snap.locale);
    metrics += F("\",device_power=\"");
    appendPrometheusLabelValue(metrics, snap.devicePower);
    metrics += F("\"} 1\n");

    if (snap.tubeRate.length())
    {
        metrics += F("# HELP radpro_tube_rate Tube rate in clicks per minute.\n");
        metrics += F("# TYPE radpro_tube_rate gauge\n");
        metrics += F("radpro_tube_rate ");
        metrics += snap.tubeRate;
        metrics += F("\n");
    }

    if (snap.tubeDoseRate.length())
    {
        metrics += F("# HELP radpro_tube_dose_rate Tube rate in microsieverts per hour.\n");
        metrics += F("# TYPE radpro_tube_dose_rate gauge\n");
        metrics += F("radpro_tube_dose_rate ");
        metrics += snap.tubeDoseRate;
        metrics += F("\n");
    }

    if (snap.tubePulseCount.length())
    {
        metrics += F("# HELP radpro_tube_pulse_count Pulse count.\n");
        metrics += F("# TYPE radpro_tube_pulse_count counter\n");
        metrics += F("radpro_tube_pulse_count ");
        metrics += snap.tubePulseCount;
        metrics += F("\n");
    }

    if (snap.batteryVoltage.length())
    {
        metrics += F("# HELP radpro_battery_voltage Battery voltage in volts.\n");
        metrics += F("# TYPE radpro_battery_voltage gauge\n");
        metrics += F("radpro_battery_voltage ");
        metrics += snap.batteryVoltage;
        metrics += F("\n");
    }

    if (snap.batteryPercent.length())
    {
        metrics += F("# HELP radpro_battery_percent Battery charge state in percent.\n");
        metrics += F("# TYPE radpro_battery_percent gauge\n");
        metrics += F("radpro_battery_percent ");
        metrics += snap.batteryPercent;
        metrics += F("\n");
    }

    manager->server->send(200, "text/plain", metrics);
}
