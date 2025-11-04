#include "Radmon/RadmonPublisher.h"

#include <WiFi.h>

namespace
{
    constexpr const char *kHost = "radmon.org";
    constexpr uint16_t kPort = 80;
    constexpr unsigned long kMinPublishGapMs = 60000;
    constexpr unsigned long kRetryBackoffMs = 60000;
}

RadmonPublisher::RadmonPublisher(AppConfig &config, Print &log, const char *bridgeVersion)
    : config_(config),
      log_(log),
      bridgeVersion_(bridgeVersion ? bridgeVersion : "")
{
}

void RadmonPublisher::begin()
{
    updateConfig();
}

void RadmonPublisher::updateConfig()
{
    // placeholder for future dynamic config
}

void RadmonPublisher::loop()
{
    publishPending();
}

void RadmonPublisher::onCommandResult(DeviceManager::CommandType type, const String &value)
{
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
    if (!publishQueued_)
        return false;

    if (!isEnabled())
        return true;

    if (!haveCpm_)
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
    log_.println(query);

    lastAttemptMs_ = now;
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
    return true;
}

bool RadmonPublisher::sendRequest(const String &query)
{
    WiFiClient client;
    if (!client.connect(kHost, kPort))
    {
        log_.println("Radmon: connect failed.");
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
        return false;
    }

    String status = client.readStringUntil('\n');
    status.trim();
    if (!status.startsWith("HTTP/1.1 "))
    {
        log_.print("Radmon: unexpected status line: ");
        log_.println(status);
        return false;
    }

    int statusCode = status.substring(9).toInt();
    if (statusCode < 200 || statusCode >= 300)
    {
        log_.print("Radmon: HTTP ");
        log_.println(statusCode);
        return false;
    }

    while (client.connected())
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
