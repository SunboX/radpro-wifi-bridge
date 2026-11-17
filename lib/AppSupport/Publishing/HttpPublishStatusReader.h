// SPDX-FileCopyrightText: 2026 André Fiedler
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "Publishing/HttpPublishResponse.h"
#include <cstdint>

namespace HttpPublishResponse
{
// Poll from the main loop: waiting for the server must not stop USB polling,
// MQTT telemetry, or the web interface. One deadline covers the whole line,
// including gaps between TCP/TLS fragments.
class StatusReader
{
public:
    static constexpr size_t kMaxStatusBytes = 160;

    void begin(uint32_t now, uint32_t timeoutMs)
    {
        startedAt_ = now;
        timeoutMs_ = timeoutMs;
        length_ = 0;
        done_ = false;
        result_ = Result{};
    }

    template <typename Client>
    bool poll(Client &client, uint32_t now)
    {
        if (done_)
            return true;
        for (size_t i = 0; i <= kMaxStatusBytes; ++i)
        {
            const int available = client.available();
            if (available < 0)
                return finish(FailureKind::ReadError);
            if (available == 0)
                break;
            const int ch = client.read();
            if (ch < 0)
                return finish(FailureKind::ReadError);
            if (ch == '\n')
                return finishLine();
            if (ch == '\r')
                continue;
            if (length_ == kMaxStatusBytes)
                return finish(FailureKind::InvalidStatusLine);
            line_[length_++] = static_cast<char>(ch);
        }
        if (static_cast<uint32_t>(now - startedAt_) >= timeoutMs_)
            return finish(FailureKind::NoResponse);
        if (!client.connected())
            return finish(FailureKind::NoResponse);
        return false;
    }

    const Result &result() const { return result_; }

private:
    bool finish(FailureKind failure)
    {
        line_[length_] = '\0';
        result_.statusLine = line_;
        result_.trace = line_;
        result_.failure = failure;
        done_ = true;
        return true;
    }

    bool finishLine()
    {
        finish(FailureKind::InvalidStatusLine);
        if (length_ < 12 ||
            (std::strncmp(line_, "HTTP/1.1 ", 9) != 0 && std::strncmp(line_, "HTTP/1.0 ", 9) != 0) ||
            line_[9] < '1' || line_[9] > '5' ||
            line_[10] < '0' || line_[10] > '9' ||
            line_[11] < '0' || line_[11] > '9' ||
            (length_ > 12 && line_[12] != ' '))
            return true;
        result_.statusCode = (line_[9] - '0') * 100 + (line_[10] - '0') * 10 + line_[11] - '0';
        result_.success = result_.statusCode >= 200 && result_.statusCode < 300;
        result_.failure = result_.success ? FailureKind::None : FailureKind::HttpError;
        return true;
    }

    char line_[kMaxStatusBytes + 1] = {};
    size_t length_ = 0;
    uint32_t startedAt_ = 0;
    uint32_t timeoutMs_ = 0;
    bool done_ = false;
    Result result_;
};
} // namespace HttpPublishResponse
