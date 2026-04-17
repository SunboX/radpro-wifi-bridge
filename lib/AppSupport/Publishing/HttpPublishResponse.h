#pragma once

#include <Arduino.h>
#include <cstdlib>
#include <cstring>

namespace HttpPublishResponse
{
enum class FailureKind
{
    None,
    NoResponse,
    InvalidStatusLine,
    HttpError,
    ReadError,
};

struct Result
{
    bool success = false;
    int statusCode = 0;
    String statusLine;
    String trace;
    FailureKind failure = FailureKind::None;
};

inline const char *failureText(FailureKind failure)
{
    switch (failure)
    {
    case FailureKind::None:
        return "none";
    case FailureKind::NoResponse:
        return "no response";
    case FailureKind::InvalidStatusLine:
        return "invalid status line";
    case FailureKind::HttpError:
        return "http error";
    case FailureKind::ReadError:
        return "read error";
    }
    return "unknown";
}

inline bool isHttpStatusLine(const String &line)
{
    return line.startsWith("HTTP/");
}

inline int parseStatusCode(const String &line)
{
    const char *raw = line.c_str();
    if (!raw)
        return 0;

    const char *space = std::strchr(raw, ' ');
    if (!space)
        return 0;

    return std::atoi(space + 1);
}

template <typename Client, typename NowFn, typename YieldFn>
FailureKind waitForReadable(Client &client,
                            unsigned long timeoutMs,
                            NowFn nowFn,
                            YieldFn yieldFn)
{
    const unsigned long startedAt = nowFn();
    while ((nowFn() - startedAt) < timeoutMs)
    {
        const int available = client.available();
        if (available < 0)
            return FailureKind::ReadError;
        if (available > 0)
            return FailureKind::None;

        yieldFn();
    }

    return FailureKind::NoResponse;
}

template <typename Client>
String readLine(Client &client, size_t maxTraceBytes, String &trace)
{
    String line;
    while (client.available() > 0)
    {
        const int ch = client.read();
        if (ch < 0)
            break;

        if (ch == '\n')
            break;
        if (ch == '\r')
            continue;

        const char c = static_cast<char>(ch);
        const char text[2] = {c, '\0'};
        line += text;
        if (trace.length() < maxTraceBytes)
            trace += text;
    }

    return line;
}

template <typename Client, typename NowFn, typename YieldFn>
Result readStatus(Client &client,
                  unsigned long timeoutMs,
                  NowFn nowFn,
                  YieldFn yieldFn,
                  size_t maxTraceBytes = 160)
{
    Result result;
    result.failure = waitForReadable(client, timeoutMs, nowFn, yieldFn);
    if (result.failure != FailureKind::None)
        return result;

    result.statusLine = readLine(client, maxTraceBytes, result.trace);
    result.statusLine.trim();
    result.trace.trim();

    if (!isHttpStatusLine(result.statusLine))
    {
        result.failure = FailureKind::InvalidStatusLine;
        return result;
    }

    result.statusCode = parseStatusCode(result.statusLine);
    if (result.statusCode >= 200 && result.statusCode < 300)
    {
        result.success = true;
        result.failure = FailureKind::None;
        return result;
    }

    result.failure = FailureKind::HttpError;
    return result;
}
} // namespace HttpPublishResponse
