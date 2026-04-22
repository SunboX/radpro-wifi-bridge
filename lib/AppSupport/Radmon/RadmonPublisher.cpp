#include "Radmon/RadmonPublisher.h"

#include <WiFi.h>
#include "ConfigPortal/WiFiPortalService.h"
#include <WebServer.h>
#include "Led/LedController.h"
#include "Publishing/HttpPublishResponse.h"
#include "Radmon/RadmonLogRedaction.h"
#include "Radmon/RadmonPortalLinks.h"
#include "Runtime/CooperativePump.h"

namespace
{
    constexpr const char *kHost = "radmon.org";
    constexpr uint16_t kPort = 80;
    constexpr unsigned long kMinPublishGapMs = 60000;
    constexpr unsigned long kRetryBackoffMs = 60000;
}

RadmonPublisher::RadmonPublisher(AppConfig &config, Print &log, const char *bridgeVersion, PublisherHealth &health)
    : config_(config),
      log_(log),
      bridgeVersion_(bridgeVersion ? bridgeVersion : ""),
      health_(health)
{
}

void RadmonPublisher::begin()
{
    updateConfig();
    syncHealthState();
}

void RadmonPublisher::updateConfig()
{
    // placeholder for future dynamic config
    syncHealthState();
}

void RadmonPublisher::loop()
{
    syncHealthState();
    if (paused_)
        return;
    publishPending();
}

void RadmonPublisher::clearPendingData()
{
    pendingCpm_ = "";
    pendingUsv_ = "";
    haveCpm_ = false;
    haveUsv_ = false;
    publishQueued_ = false;
    suppressUntilMs_ = 0;
    syncHealthState();
}

void RadmonPublisher::onCommandResult(DeviceManager::CommandType type, const String &value)
{
    if (paused_)
        return;
    switch (type)
    {
    case DeviceManager::CommandType::TubeRate:
        if (value.length())
        {
            pendingCpm_ = value;
            haveCpm_ = true;
        }
        break;
    case DeviceManager::CommandType::TubeDoseRate:
        if (value.length())
        {
            pendingUsv_ = value;
            haveUsv_ = true;
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

bool RadmonPublisher::isEnabled() const
{
    if (!config_.radmonEnabled)
        return false;
    if (!config_.radmonUser.length())
        return false;
    if (!config_.radmonPassword.length())
        return false;
    return true;
}

bool RadmonPublisher::publishPending()
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

    if (!haveCpm_)
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

    String query;
    query.reserve(160);
    query = "/radmon.php?function=submit&user=";
    query += urlEncode(config_.radmonUser);
    query += "&password=";
    query += urlEncode(config_.radmonPassword);
    query += "&value=";
    query += pendingCpm_;
    query += "&unit=CPM";
    if (haveUsv_ && pendingUsv_.length())
    {
        query += "&value2=";
        query += pendingUsv_;
        query += "&unit2=uSv/h";
    }

    log_.print("Radmon: GET ");
    log_.println(RadmonLogRedaction::redactQueryForLogs(query));

    lastAttemptMs_ = now;
    health_.noteAttempt(now);
    bool ok = sendRequest(query);
    if (ok)
    {
        publishQueued_ = false;
        haveCpm_ = false;
        haveUsv_ = false;
        lastAttemptMs_ = millis();
    }
    else
    {
        suppressUntilMs_ = millis() + kRetryBackoffMs;
    }
    syncHealthState();
    return true;
}

void RadmonPublisher::syncHealthState()
{
    health_.setEnabled(isEnabled());
    health_.setPaused(paused_);
    health_.setPending(publishQueued_);
}

bool RadmonPublisher::sendRequest(const String &query)
{
    WiFiClient client;
    client.setTimeout(10000);
    if (!client.connect(kHost, kPort))
    {
        log_.println("Radmon: connect failed.");
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
        log_.println("Radmon: send failed.");
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
            log_.print("Radmon: no response before ");
            if (client.connected())
                log_.println("timeout");
            else
                log_.println("disconnect");
            health_.noteFailure(millis(), "no response", 0, response.statusLine, response.trace);
            break;
        case HttpPublishResponse::FailureKind::InvalidStatusLine:
            log_.print("Radmon: unexpected status line: ");
            log_.println(response.statusLine.length() ? response.statusLine : String("<empty>"));
            if (response.trace.length())
            {
                log_.print("Radmon: response trace: ");
                log_.println(response.trace);
            }
            health_.noteFailure(millis(), "invalid status line", 0, response.statusLine, response.trace);
            break;
        case HttpPublishResponse::FailureKind::HttpError:
            log_.print("Radmon: HTTP ");
            log_.println(response.statusCode);
            health_.noteFailure(millis(), "http error", response.statusCode, response.statusLine, response.trace);
            break;
        case HttpPublishResponse::FailureKind::ReadError:
            log_.println("Radmon: response read error.");
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

String RadmonPublisher::urlEncode(const String &input)
{
    String encoded;
    encoded.reserve(input.length());
    for (size_t i = 0; i < input.length(); ++i)
    {
        char c = input[i];
        if ((c >= '0' && c <= '9') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            c == '-' || c == '_' || c == '.' || c == '~')
        {
            encoded += c;
        }
        else
        {
            char buf[4];
            snprintf(buf, sizeof(buf), "%%%02X", static_cast<unsigned char>(c));
            encoded += buf;
        }
    }
    return encoded;
}

void RadmonPublisher::HandlePortalPost(WebServer &server,
                                       AppConfig &config,
                                       AppConfigStore &store,
                                       LedController &led,
                                       Print &log,
                                       String &message)
{
    bool enabled = server.hasArg("radmonEnabled") && server.arg("radmonEnabled") == "1";
    String user = server.arg("radmonUser");
    String password = server.arg("radmonPass");

    user.trim();

    bool changed = false;
    if (config.radmonEnabled != enabled)
    {
        config.radmonEnabled = enabled;
        changed = true;
    }
    changed |= UpdateStringIfChanged(config.radmonUser, user.c_str());

    if (password != config.radmonPassword)
    {
        config.radmonPassword = password;
        changed = true;
    }

    if (changed)
    {
        if (store.save(config))
        {
            log.println("Radmon configuration saved to NVS.");
            led.clearFault(FaultCode::NvsWriteFailure);
            message = F("Radmon settings saved.");
        }
        else
        {
            log.println("Preferences write failed; Radmon configuration not saved.");
            led.activateFault(FaultCode::NvsWriteFailure);
            message = F("Failed to save settings.");
        }
        return;
    }

    message = F("No changes detected.");
}

void RadmonPublisher::SendPortalForm(WiFiPortalService &portal, const String &message)
{
    if (!portal.manager_.server)
        return;

    String notice = WiFiPortalService::htmlEscape(message);
    const String stationUrl = RadmonPortalLinks::buildRadmonStationUrl(
        portal.config_.radmonUser);
    WiFiPortalService::TemplateReplacements vars = {
        {"{{NOTICE_CLASS}}", notice.length() ? String() : String("hidden")},
        {"{{NOTICE_TEXT}}", notice},
        {"{{RADMON_ENABLED_CHECKED}}", portal.config_.radmonEnabled ? String("checked") : String()},
        {"{{RADMON_USER}}", WiFiPortalService::htmlEscape(portal.config_.radmonUser)},
        {"{{RADMON_PASS}}", WiFiPortalService::htmlEscape(portal.config_.radmonPassword)},
        {"{{RADMON_STATION_LINK_CLASS}}", stationUrl.length() ? String() : String("hidden")},
        {"{{RADMON_STATION_URL}}", WiFiPortalService::htmlEscape(stationUrl)}};

    portal.appendCommonTemplateVars(vars);
    portal.sendTemplate("/portal/radmon.html", vars);
}
