#pragma once

#include <Arduino.h>
#include <cstdint>
#include <cstdio>

namespace SafecastProtocol
{
static constexpr const char *kProductionApiBaseUrl = "https://api.safecast.org";
static constexpr const char *kTestApiBaseUrl = "https://dev.safecast.org";
static constexpr const char *kMeasurementPath = "/measurements.json";
static constexpr const char *kContentType = "application/json";

struct Endpoint
{
    bool secure = true;
    uint16_t port = 443;
    String host;
    String basePath;
    String baseUrl;
};

inline String trimCopy(const String &value)
{
    String trimmed = value;
    trimmed.trim();
    return trimmed;
}

inline String trimTrailingSlash(const String &value)
{
    String normalized = trimCopy(value);
    while (normalized.length() > 1 && normalized[normalized.length() - 1] == '/')
        normalized = normalized.substring(0, normalized.length() - 1);
    return normalized;
}

inline String normalizeBaseUrl(const String &value)
{
    return trimTrailingSlash(value);
}

inline String resolveBaseUrl(const String &productionBaseUrl,
                             bool useTestApi,
                             const String &customBaseUrl)
{
    const String normalizedCustom = normalizeBaseUrl(customBaseUrl);
    if (normalizedCustom.length())
        return normalizedCustom;

    if (useTestApi)
        return normalizeBaseUrl(String(kTestApiBaseUrl));

    const String normalizedProduction = normalizeBaseUrl(productionBaseUrl);
    if (normalizedProduction.length())
        return normalizedProduction;

    return String(kProductionApiBaseUrl);
}

inline String urlEncode(const String &input)
{
    String encoded;
    for (size_t i = 0; i < input.length(); ++i)
    {
        const char c = input[i];
        if ((c >= '0' && c <= '9') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            c == '-' || c == '_' || c == '.' || c == '~')
        {
            encoded += c;
        }
        else
        {
            char buffer[4];
            std::snprintf(buffer, sizeof(buffer), "%%%02X", static_cast<unsigned char>(c));
            encoded += buffer;
        }
    }
    return encoded;
}

inline bool parseBaseUrl(const String &value, Endpoint &out)
{
    const String normalized = normalizeBaseUrl(value);
    const int schemeEnd = normalized.indexOf("://");
    if (schemeEnd <= 0)
        return false;

    const String scheme = normalized.substring(0, schemeEnd);
    const bool secure = scheme.equalsIgnoreCase("https");
    if (!secure && !scheme.equalsIgnoreCase("http"))
        return false;

    const String authorityAndPath = normalized.substring(schemeEnd + 3);
    if (!authorityAndPath.length())
        return false;

    const int pathStart = authorityAndPath.indexOf('/');
    String authority = pathStart >= 0 ? authorityAndPath.substring(0, pathStart) : authorityAndPath;
    String path = pathStart >= 0 ? authorityAndPath.substring(pathStart) : String();
    authority.trim();
    if (!authority.length())
        return false;

    uint16_t port = secure ? 443 : 80;
    const int portSeparator = authority.lastIndexOf(':');
    if (portSeparator > 0 && portSeparator + 1 < static_cast<int>(authority.length()) && authority.indexOf(']') < 0)
    {
        const String portText = authority.substring(portSeparator + 1);
        const long parsedPort = portText.toInt();
        if (parsedPort <= 0 || parsedPort > 65535)
            return false;
        port = static_cast<uint16_t>(parsedPort);
        authority = authority.substring(0, portSeparator);
    }

    path = trimTrailingSlash(path);
    if (path == "/")
        path = String();

    out.secure = secure;
    out.port = port;
    out.host = authority;
    out.basePath = path;
    out.baseUrl = normalized;
    return out.host.length() > 0;
}

inline String buildMeasurementPath(const Endpoint &endpoint, const String &apiKey)
{
    String path = endpoint.basePath;
    path += kMeasurementPath;
    path += "?api_key=";
    path += urlEncode(apiKey);
    return path;
}

inline String buildMeasurementUrl(const String &resolvedBaseUrl, const String &apiKey)
{
    String url = normalizeBaseUrl(resolvedBaseUrl);
    url += kMeasurementPath;
    url += "?api_key=";
    url += urlEncode(apiKey);
    return url;
}
} // namespace SafecastProtocol
