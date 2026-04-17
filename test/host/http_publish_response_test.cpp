#include <cassert>
#include <iostream>
#include <vector>

#include "Publishing/HttpPublishResponse.h"

namespace
{
struct FakeClient
{
    std::vector<int> availableSequence;
    std::vector<bool> connectedSequence;
    std::string payload;
    size_t availableIndex = 0;
    size_t connectedIndex = 0;
    size_t readIndex = 0;

    int available()
    {
        if (availableIndex < availableSequence.size())
            return availableSequence[availableIndex++];
        return availableSequence.empty() ? 0 : availableSequence.back();
    }

    bool connected()
    {
        if (connectedIndex < connectedSequence.size())
            return connectedSequence[connectedIndex++];
        return connectedSequence.empty() ? false : connectedSequence.back();
    }

    int read()
    {
        if (readIndex >= payload.size())
            return -1;
        return static_cast<unsigned char>(payload[readIndex++]);
    }
};

void testAcceptsHttp10StatusAfterDelayedReadableBytes()
{
    FakeClient client{
        {0, 0, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0},
        {true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, true, false},
        "HTTP/1.0 200 OK\r\nHeader: x\r\n\r\n"};

    unsigned long now = 0;
    const auto result = HttpPublishResponse::readStatus(
        client,
        100,
        [&now]() { return now; },
        [&now]() { now += 10; });

    assert(result.success);
    assert(result.failure == HttpPublishResponse::FailureKind::None);
    assert(result.statusCode == 200);
    assert(std::string(result.statusLine.c_str()) == "HTTP/1.0 200 OK");
    assert(std::string(result.trace.c_str()) == "HTTP/1.0 200 OK");
}

void testMalformedStatusCapturesTrace()
{
    FakeClient client{{5, 4, 3, 2, 1, 0}, {true, true, true, true, true, false}, "OK\r\n"};
    unsigned long now = 0;

    const auto result = HttpPublishResponse::readStatus(
        client,
        100,
        [&now]() { return now; },
        [&now]() { now += 10; });

    assert(!result.success);
    assert(result.failure == HttpPublishResponse::FailureKind::InvalidStatusLine);
    assert(result.statusCode == 0);
    assert(std::string(result.statusLine.c_str()) == "OK");
    assert(std::string(result.trace.c_str()) == "OK");
}

void testNoResponseBeforeDisconnectIsReported()
{
    FakeClient client{{0, 0, 0}, {true, false, false}, ""};
    unsigned long now = 0;

    const auto result = HttpPublishResponse::readStatus(
        client,
        25,
        [&now]() { return now; },
        [&now]() { now += 10; });

    assert(!result.success);
    assert(result.failure == HttpPublishResponse::FailureKind::NoResponse);
    assert(result.statusCode == 0);
    assert(std::string(result.statusLine.c_str()).empty());
    assert(std::string(result.trace.c_str()).empty());
}
} // namespace

int main()
{
    testAcceptsHttp10StatusAfterDelayedReadableBytes();
    testMalformedStatusCapturesTrace();
    testNoResponseBeforeDisconnectIsReported();
    std::cout << "http publish response tests passed\n";
    return 0;
}
