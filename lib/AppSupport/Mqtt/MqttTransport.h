// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <WiFiClient.h>

class MqttTransport : public WiFiClient
{
public:
    using WiFiClient::write;
    size_t write(const uint8_t *data, size_t size) override;
    uint8_t connected() override;
    void stop() override;
    struct WriteFailure
    {
        uint32_t at = 0, elapsedMs = 0;
        int error = 0;
    };
    bool takeWriteFailure(WriteFailure &failure);

private:
    WriteFailure failure_;
};
