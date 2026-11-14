// SPDX-FileCopyrightText: 2026 André Fiedler
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <cassert>
#include <iostream>
#include <limits>
#include <string>

#include "OpenSenseMap/OpenSenseMapRequest.h"

namespace
{
struct Client
{
    std::string wire;
    size_t calls = 0;
    size_t limit = std::numeric_limits<size_t>::max();

    size_t write(const uint8_t *bytes, size_t size)
    {
        ++calls;
        const size_t sent = std::min(size, limit);
        wire.append(reinterpret_cast<const char *>(bytes), sent);
        return sent;
    }
};

JsonDocument measurements()
{
    JsonDocument payload;
    assert(!deserializeJson(payload,
        R"([{"sensor":"rate-id","value":"25.526"},{"sensor":"dose-id","value":"0.23560"}])"));
    return payload;
}

void testCompleteHttpRequestUsesOneWrite()
{
    Client client;
    const auto payload = measurements();
    assert(OpenSenseMapRequest::send(client, payload, "box-id", "secret", "1.15.20") ==
           OpenSenseMapRequest::Result::Sent);
    const std::string body =
        R"([{"sensor":"rate-id","value":"25.526"},{"sensor":"dose-id","value":"0.23560"}])";
    const std::string expected =
        "POST /boxes/box-id/data HTTP/1.1\r\n"
        "Host: api.opensensemap.org\r\n"
        "Connection: close\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: " + std::to_string(body.size()) + "\r\n"
        "Authorization: secret\r\n"
        "User-Agent: RadPro-WiFi-Bridge/1.15.20\r\n\r\n" + body;
    assert(client.calls == 1);
    assert(client.wire == expected);
}

void testShortWritesAreFailuresWithoutPrintErrorFlag()
{
    const auto payload = measurements();
    Client reference;
    assert(OpenSenseMapRequest::send(reference, payload, "box-id", "secret", "1.15.20") ==
           OpenSenseMapRequest::Result::Sent);
    for (size_t limit : {size_t(0), reference.wire.size() - 1})
    {
        Client client;
        client.limit = limit;
        assert(OpenSenseMapRequest::send(client, payload, "box-id", "secret", "1.15.20") ==
               OpenSenseMapRequest::Result::WriteFailed);
        assert(client.calls == 1);
        assert(client.wire.size() == limit);
    }
}

void testCapacityBoundaryAndOversizedPayloadDoNotSendTruncatedJson()
{
    const auto payload = measurements();
    Client normal;
    assert(OpenSenseMapRequest::send(normal, payload, "box-id", "secret", "1.15.20") ==
           OpenSenseMapRequest::Result::Sent);
    std::string key = "secret";
    key.append(OpenSenseMapRequest::kCapacity - 1 - normal.wire.size(), 'x');
    Client exact;
    assert(OpenSenseMapRequest::send(exact, payload, "box-id", key.c_str(), "1.15.20") ==
           OpenSenseMapRequest::Result::Sent);
    assert(exact.wire.size() == OpenSenseMapRequest::kCapacity - 1);

    key += 'x';
    Client oversized;
    assert(OpenSenseMapRequest::send(oversized, payload, "box-id", key.c_str(), "1.15.20") ==
           OpenSenseMapRequest::Result::TooLarge);
    assert(oversized.calls == 0);

    JsonDocument largePayload;
    largePayload["value"] = std::string(OpenSenseMapRequest::kCapacity, 'x');
    assert(OpenSenseMapRequest::send(oversized, largePayload, "box-id", "secret", "1.15.20") ==
           OpenSenseMapRequest::Result::TooLarge);
    assert(oversized.calls == 0);
}
} // namespace

int main()
{
    testCompleteHttpRequestUsesOneWrite();
    testShortWritesAreFailuresWithoutPrintErrorFlag();
    testCapacityBoundaryAndOversizedPayloadDoNotSendTruncatedJson();
    std::cout << "OpenSenseMap request tests passed\n";
}
