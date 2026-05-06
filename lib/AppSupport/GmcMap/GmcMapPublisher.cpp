// SPDX-FileCopyrightText: 2026 André Fiedler
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "GmcMap/GmcMapPublisher.h"

#include <WiFi.h>
#include "ConfigPortal/WiFiPortalService.h"
#include <WebServer.h>
#include "Led/LedController.h"
#include <WiFiClient.h>
#include <cmath>
#include "GmcMap/GmcMapLogRedaction.h"
#include "GmcMap/GmcMapPortalLinks.h"
#include "GmcMap/GmcMapPayload.h"
#include "Publishing/HttpPublishResponse.h"
#include "Runtime/CooperativePump.h"

namespace
{
    constexpr const char *kHost = "www.gmcmap.com";
    constexpr uint16_t kPort = 80;
    constexpr unsigned long kMinPublishGapMs = 60000; // GMCMap recommends ~60s interval
    constexpr unsigned long kRetryBackoffMs = 60000;
    constexpr unsigned long kAcpmWindowMs = 60000;
}

GmcMapPublisher::GmcMapPublisher(AppConfig &config, Print &log, const char *bridgeVersion, PublisherHealth &health)
    : config_(config),
      log_(log),
      bridgeVersion_(bridgeVersion ? bridgeVersion : ""),
      health_(health)
{
}

void GmcMapPublisher::begin()
{
    updateConfig();
    syncHealthState();
}

void GmcMapPublisher::updateConfig()
{
    // nothing dynamic yet – placeholder for future settings
    syncHealthState();
}

void GmcMapPublisher::loop()
{
    syncHealthState();
    if (paused_)
        return;
    publishPending();
}

void GmcMapPublisher::clearPendingData()
{
    pendinguSv_ = "";
    pendingCpmValue_ = 0.0f;
    haveCpm_ = false;
    haveuSv_ = false;
    publishQueued_ = false;
    suppressUntilMs_ = 0;
    rateSamples_.clear();
    rateSampleSum_ = 0.0f;
    syncHealthState();
}

void GmcMapPublisher::onCommandResult(DeviceManager::CommandType type, const String &value)
{
    if (paused_)
        return;
    switch (type)
    {
    case DeviceManager::CommandType::TubeRate:
        if (value.length())
        {
            haveCpm_ = true;
            pendingCpmValue_ = value.toFloat();
            addRateSample(pendingCpmValue_, millis());
        }
        break;
    case DeviceManager::CommandType::TubeDoseRate:
        if (value.length())
        {
            pendinguSv_ = value;
            haveuSv_ = true;
            if (haveCpm_)
            {
                publishQueued_ = true;
                suppressUntilMs_ = 0;
            }
        }
        break;
    default:
        break;
    }
    syncHealthState();
}

bool GmcMapPublisher::isEnabled() const
{
    if (!config_.gmcMapEnabled)
        return false;
    if (!config_.gmcMapAccountId.length())
        return false;
    if (!config_.gmcMapDeviceId.length())
        return false;
    return true;
}

bool GmcMapPublisher::publishPending()
{
    if (paused_)
    {
        syncHealthState();
        return true;
    }
    if (!publishQueued_)
    {
        syncHealthState();
        return false;
    }

    if (!isEnabled())
    {
        syncHealthState();
        return true;
    }

    if (!haveCpm_ || !haveuSv_)
    {
        publishQueued_ = false;
        syncHealthState();
        return true;
    }

    if (WiFi.status() != WL_CONNECTED)
    {
        syncHealthState();
        return true;
    }

    unsigned long now = millis();
    if (suppressUntilMs_ && now < suppressUntilMs_)
    {
        syncHealthState();
        return true;
    }
    if (now - lastAttemptMs_ < kMinPublishGapMs)
    {
        syncHealthState();
        return true;
    }

    float acpmValue = 0.0f;
    const float acpmForQuery = computeAcpm(acpmValue) ? acpmValue : pendingCpmValue_;
    const String query = GmcMapPayload::buildLogQuery(
        config_.gmcMapAccountId,
        config_.gmcMapDeviceId,
        pendingCpmValue_,
        acpmForQuery,
        pendinguSv_);

    log_.print("GMCMap: GET ");
    log_.println(GmcMapLogRedaction::redactQueryForLogs(query));

    lastAttemptMs_ = now;
    health_.noteAttempt(now);
    bool ok = sendRequest(query);
    if (ok)
    {
        publishQueued_ = false;
        haveCpm_ = false;
        haveuSv_ = false;
        lastAttemptMs_ = millis();
    }
    else
    {
        suppressUntilMs_ = millis() + kRetryBackoffMs;
    }
    syncHealthState();
    return true;
}

void GmcMapPublisher::syncHealthState()
{
    health_.setEnabled(isEnabled());
    health_.setPaused(paused_);
    health_.setPending(publishQueued_);
}

void GmcMapPublisher::addRateSample(float cpm, unsigned long now)
{
    if (!std::isfinite(cpm))
        return;
    rateSamples_.push_back({now, cpm});
    rateSampleSum_ += cpm;
    pruneSamples(now);
}

void GmcMapPublisher::pruneSamples(unsigned long now)
{
    bool modified = false;
    while (!rateSamples_.empty())
    {
        unsigned long age = now - rateSamples_.front().timestamp;
        if (age <= kAcpmWindowMs)
            break;
        rateSampleSum_ -= rateSamples_.front().cpm;
        rateSamples_.erase(rateSamples_.begin());
        modified = true;
    }
    if (modified && rateSamples_.empty())
    {
        rateSampleSum_ = 0.0f;
    }
}

bool GmcMapPublisher::computeAcpm(float &out)
{
    unsigned long now = millis();
    pruneSamples(now);
    if (rateSamples_.empty())
    {
        return false;
    }
    if (rateSampleSum_ <= 0.0f)
    {
        // allow zero CPM average but clamp tiny negative values from float drift
        if (rateSampleSum_ < 0.0f)
            rateSampleSum_ = 0.0f;
    }
    out = rateSampleSum_ / static_cast<float>(rateSamples_.size());
    return true;
}

void GmcMapPublisher::HandlePortalPost(WebServer &server,
                                       AppConfig &config,
                                       AppConfigStore &store,
                                       LedController &led,
                                       Print &log,
                                       String &message)
{
    bool enabled = server.hasArg("gmcEnabled") && server.arg("gmcEnabled") == "1";
    String account = server.arg("gmcAccount");
    String device = server.arg("gmcDevice");

    account.trim();
    device.trim();

    bool changed = false;
    if (config.gmcMapEnabled != enabled)
    {
        config.gmcMapEnabled = enabled;
        changed = true;
    }
    changed |= UpdateStringIfChanged(config.gmcMapAccountId, account.c_str());
    changed |= UpdateStringIfChanged(config.gmcMapDeviceId, device.c_str());

    if (changed)
    {
        if (store.save(config))
        {
            log.println("GMCMap configuration saved to NVS.");
            led.clearFault(FaultCode::NvsWriteFailure);
            message = F("GMCMap settings saved.");
        }
        else
        {
            log.println("Preferences write failed; GMCMap configuration not saved.");
            led.activateFault(FaultCode::NvsWriteFailure);
            message = F("Failed to save settings.");
        }
        return;
    }

    message = F("No changes detected.");
}

void GmcMapPublisher::SendPortalForm(WiFiPortalService &portal, const String &message)
{
    if (!portal.manager_.server)
        return;

    String notice = WiFiPortalService::htmlEscape(message);
    const String deviceHistoryUrl = GmcMapPortalLinks::buildGmcMapDeviceHistoryUrl(
        portal.config_.gmcMapDeviceId);
    WiFiPortalService::TemplateReplacements vars = {
        {"{{NOTICE_CLASS}}", notice.length() ? String() : String("hidden")},
        {"{{NOTICE_TEXT}}", notice},
        {"{{GMC_ENABLED_CHECKED}}", portal.config_.gmcMapEnabled ? String("checked") : String()},
        {"{{GMC_ACCOUNT}}", WiFiPortalService::htmlEscape(portal.config_.gmcMapAccountId)},
        {"{{GMC_DEVICE}}", WiFiPortalService::htmlEscape(portal.config_.gmcMapDeviceId)},
        {"{{GMC_DEVICE_LINK_CLASS}}", deviceHistoryUrl.length() ? String() : String("hidden")},
        {"{{GMC_DEVICE_URL}}", WiFiPortalService::htmlEscape(deviceHistoryUrl)}};

    portal.appendCommonTemplateVars(vars);
    portal.sendTemplate("/portal/gmc.html", vars);
}

bool GmcMapPublisher::sendRequest(const String &query)
{
    WiFiClient client;
    client.setTimeout(10);
    if (!client.connect(kHost, kPort))
    {
        log_.println("GMCMap: connect failed.");
        health_.noteFailure(millis(), "connect failed");
        return false;
    }

    String request;
    request.reserve(query.length() + 80);
    request += "GET ";
    request += query;
    request += " HTTP/1.1\r\nHost: ";
    request += kHost;
    request += "\r\nConnection: close\r\nUser-Agent: RadPro-WiFi-Bridge/";
    request += bridgeVersion_;
    request += "\r\n\r\n";

    if (client.print(request) != request.length())
    {
        log_.println("GMCMap: send failed.");
        health_.noteFailure(millis(), "send failed");
        return false;
    }

    client.flush();

    const auto response = HttpPublishResponse::readStatus(
        client,
        10000,
        []() { return millis(); },
        []() { CooperativePump::service(); });

    if (!response.success)
    {
        switch (response.failure)
        {
        case HttpPublishResponse::FailureKind::NoResponse:
            log_.print("GMCMap: no response before ");
            if (client.connected())
                log_.println("timeout");
            else
                log_.println("disconnect");
            health_.noteFailure(millis(), "no response", 0, response.statusLine, response.trace);
            break;
        case HttpPublishResponse::FailureKind::InvalidStatusLine:
            log_.print("GMCMap: unexpected status line: ");
            log_.println(response.statusLine.length() ? response.statusLine : String("<empty>"));
            if (response.trace.length())
            {
                log_.print("GMCMap: response trace: ");
                log_.println(response.trace);
            }
            health_.noteFailure(millis(), "invalid status line", 0, response.statusLine, response.trace);
            break;
        case HttpPublishResponse::FailureKind::HttpError:
            log_.print("GMCMap: HTTP ");
            log_.println(response.statusCode);
            if (response.trace.length())
            {
                log_.print("GMCMap: response trace: ");
                log_.println(response.trace);
            }
            health_.noteFailure(millis(), "http error", response.statusCode, response.statusLine, response.trace);
            break;
        case HttpPublishResponse::FailureKind::ReadError:
            log_.println("GMCMap: response read error.");
            health_.noteFailure(millis(), "read error", 0, response.statusLine, response.trace);
            break;
        case HttpPublishResponse::FailureKind::None:
            break;
        }
        return false;
    }

    health_.noteSuccess(millis(), response.statusCode, response.statusLine);

    while (client.connected() || client.available())
        client.read();

    return true;
}
