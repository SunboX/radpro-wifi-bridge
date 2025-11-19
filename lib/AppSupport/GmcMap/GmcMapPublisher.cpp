#include "GmcMap/GmcMapPublisher.h"

#include <WiFi.h>
#include "ConfigPortal/WiFiPortalService.h"
#include <WebServer.h>
#include "Led/LedController.h"
#include <WiFiClient.h>
#include <cmath>

namespace
{
    constexpr const char *kHost = "www.gmcmap.com";
    constexpr uint16_t kPort = 80;
    constexpr unsigned long kMinPublishGapMs = 60000; // GMCMap recommends ~60s interval
    constexpr unsigned long kRetryBackoffMs = 60000;
    constexpr unsigned long kAcpmWindowMs = 60000;
}

GmcMapPublisher::GmcMapPublisher(AppConfig &config, Print &log, const char *bridgeVersion)
    : config_(config),
      log_(log),
      bridgeVersion_(bridgeVersion ? bridgeVersion : "")
{
}

void GmcMapPublisher::begin()
{
    updateConfig();
}

void GmcMapPublisher::updateConfig()
{
    // nothing dynamic yet – placeholder for future settings
}

void GmcMapPublisher::loop()
{
    publishPending();
}

void GmcMapPublisher::onCommandResult(DeviceManager::CommandType type, const String &value)
{
    switch (type)
    {
    case DeviceManager::CommandType::TubeRate:
        if (value.length())
        {
            pendingCpm_ = value;
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
    if (!publishQueued_)
        return false;

    if (!isEnabled())
        return true;

    if (!haveCpm_ || !haveuSv_)
    {
        publishQueued_ = false;
        return true;
    }

    if (WiFi.status() != WL_CONNECTED)
        return true;

    unsigned long now = millis();
    if (suppressUntilMs_ && now < suppressUntilMs_)
        return true;
    if (now - lastAttemptMs_ < kMinPublishGapMs)
        return true;

    String query;
    query.reserve(160);
    query = "/log2.asp?AID=";
    query += config_.gmcMapAccountId;
    query += "&GID=";
    query += config_.gmcMapDeviceId;
    query += "&CPM=";
    query += pendingCpm_;
    float acpmValue = 0.0f;
    String acpmString;
    if (computeAcpm(acpmValue))
    {
        acpmString = formatFloat(acpmValue);
    }
    else
    {
        acpmString = pendingCpm_;
    }
    query += "&ACPM=";
    query += acpmString;
    query += "&uSV=";
    query += pendinguSv_;

    log_.print("GMCMap: GET ");
    log_.println(query);

    lastAttemptMs_ = now;
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
    return true;
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
    WiFiPortalService::TemplateReplacements vars = {
        {"{{NOTICE_CLASS}}", notice.length() ? String() : String("hidden")},
        {"{{NOTICE_TEXT}}", notice},
        {"{{GMC_ENABLED_CHECKED}}", portal.config_.gmcMapEnabled ? String("checked") : String()},
        {"{{GMC_ACCOUNT}}", WiFiPortalService::htmlEscape(portal.config_.gmcMapAccountId)},
        {"{{GMC_DEVICE}}", WiFiPortalService::htmlEscape(portal.config_.gmcMapDeviceId)}};

    portal.appendCommonTemplateVars(vars);
    portal.sendTemplate("/portal/gmc.html", vars);
}

String GmcMapPublisher::formatFloat(float value, uint8_t decimals)
{
    String s(value, static_cast<unsigned int>(decimals));
    int dotIndex = s.indexOf('.');
    if (dotIndex < 0)
        return s;

    int end = s.length() - 1;
    while (end > dotIndex && s[end] == '0')
        --end;
    if (end == dotIndex)
        --end;
    s.remove(end + 1);
    if (s.length() == 0)
        s = "0";
    return s;
}

bool GmcMapPublisher::sendRequest(const String &query)
{
    WiFiClient client;
    if (!client.connect(kHost, kPort))
    {
        log_.println("GMCMap: connect failed.");
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
        return false;
    }

    // Read status line
    String status = client.readStringUntil('\n');
    status.trim();
    if (!status.startsWith("HTTP/1.1 "))
    {
        log_.print("GMCMap: unexpected status line: ");
        log_.println(status);
        return false;
    }

    int statusCode = status.substring(9).toInt();
    if (statusCode < 200 || statusCode >= 300)
    {
        log_.print("GMCMap: HTTP ");
        log_.println(statusCode);
        return false;
    }

    while (client.connected())
        client.read();

    return true;
}
