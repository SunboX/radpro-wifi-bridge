// SPDX-FileCopyrightText: 2026 André Fiedler
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <ArduinoJson.h>
#include <cstdio>

namespace OpenSenseMapRequest
{
constexpr const char *kHost = "api.opensensemap.org";
constexpr size_t kCapacity = 1024;

enum class Result
{
    Sent,
    TooLarge,
    WriteFailed,
};

template <typename Client>
Result send(Client &client,
            const JsonDocument &payload,
            const char *boxId,
            const char *apiKey,
            const char *bridgeVersion)
{
    // ArduinoJson writes punctuation one byte at a time to Print streams.
    // Buffer this small request so TLS sends it in one record, and check the
    // returned byte count: WiFiClientSecure does not set Print's write error.
    char request[kCapacity];
    const size_t bodyLength = measureJson(payload);
    const int headerLength = std::snprintf(
        request, sizeof(request),
        "POST /boxes/%s/data HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Connection: close\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %lu\r\n"
        "Authorization: %s\r\n"
        "User-Agent: RadPro-WiFi-Bridge/%s\r\n\r\n",
        boxId, kHost, static_cast<unsigned long>(bodyLength), apiKey, bridgeVersion);
    if (headerLength < 0 || static_cast<size_t>(headerLength) >= sizeof(request) ||
        bodyLength >= sizeof(request) - static_cast<size_t>(headerLength))
        return Result::TooLarge;

    if (serializeJson(payload, request + headerLength, sizeof(request) - headerLength) != bodyLength)
        return Result::TooLarge;

    const size_t requestLength = static_cast<size_t>(headerLength) + bodyLength;
    return client.write(reinterpret_cast<const uint8_t *>(request), requestLength) == requestLength
               ? Result::Sent
               : Result::WriteFailed;
}
} // namespace OpenSenseMapRequest
