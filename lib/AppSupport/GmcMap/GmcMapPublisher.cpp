#include "GmcMap/GmcMapPublisher.h"

#include <WiFi.h>
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

GmcMapPublisher::GmcMapPublisher(AppConfig &config, Print &log)
    : config_(config),
      log_(log)
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
    request += "\r\nConnection: close\r\nUser-Agent: RadPro-WiFi-Bridge/1.2.0\r\n\r\n";

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
