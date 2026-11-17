// SPDX-FileCopyrightText: 2026 André Fiedler
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include "Publishing/HttpPublishStatusReader.h"

struct Client
{
    std::string bytes;
    size_t offset = 0;
    bool open = true;
    bool error = false;
    int available() { return error ? -1 : static_cast<int>(bytes.size() - offset); }
    bool connected() { return open; }
    int read() { return offset < bytes.size() ? static_cast<unsigned char>(bytes[offset++]) : -1; }
};

int main()
{
    using namespace HttpPublishResponse;
    StatusReader reader;
    Client client;
    reader.begin(0, 30000);
    for (uint32_t now = 0; now < 12000; now += 10)
        assert(!reader.poll(client, now)); // Returns immediately; main-loop work can continue.
    client.bytes = "HTTP/1.1 20";
    assert(!reader.poll(client, 12000));
    assert(!reader.result().success); // A partial code must never count as success.
    client.bytes += "1 Created\r";
    assert(!reader.poll(client, 12500));
    client.bytes += "\nContent-Length: 0\r\n\r\n";
    assert(reader.poll(client, 13000));
    assert(reader.result().success && reader.result().statusCode == 201);
    assert(client.bytes.substr(client.offset).find("Content-Length") == 0);

    reader.begin(0, 30000);
    client = Client{};
    client.bytes = "HTTP/1.1 201";
    assert(!reader.poll(client, 1000));
    assert(!reader.poll(client, 29999));
    assert(reader.poll(client, 30000));
    assert(reader.result().failure == FailureKind::NoResponse);
    assert(!reader.result().success);
    assert(std::string(reader.result().trace.c_str()) == "HTTP/1.1 201");

    reader.begin(UINT32_MAX - 99, 30000);
    client = Client{};
    assert(!reader.poll(client, 29899));
    assert(reader.poll(client, 29900));
    assert(reader.result().failure == FailureKind::NoResponse);

    reader.begin(0, 30000);
    client = Client{};
    client.open = false;
    assert(reader.poll(client, 1));
    assert(reader.result().failure == FailureKind::NoResponse);

    reader.begin(0, 30000);
    client = Client{};
    client.error = true;
    assert(reader.poll(client, 1));
    assert(reader.result().failure == FailureKind::ReadError);

    for (const auto &bad : {std::string("HTTP/1.1 20 OK\r\n"),
                            std::string("HTTP/1.1 201x\r\n"),
                            std::string(StatusReader::kMaxStatusBytes + 1, 'x') + "\r\n"})
    {
        reader.begin(0, 30000);
        client = Client{};
        client.bytes = bad;
        assert(reader.poll(client, 1));
        assert(reader.result().failure == FailureKind::InvalidStatusLine);
        assert(reader.result().trace.length() <= StatusReader::kMaxStatusBytes);
    }

    for (int status : {429, 502})
    {
        reader.begin(0, 30000);
        client = Client{};
        client.bytes = "HTTP/1.1 " + std::to_string(status) + " Error\r\n";
        assert(reader.poll(client, 1));
        assert(reader.result().failure == FailureKind::HttpError);
        assert(reader.result().statusCode == status);
    }

    reader.begin(0, 60000);
    client = Client{};
    assert(!reader.poll(client, 35000));
    client.bytes = "HTTP/1.1 201 Created\r\n";
    assert(reader.poll(client, 36000));
    assert(reader.result().success);

    reader.begin(0, 60000);
    client = Client{};
    assert(!reader.poll(client, 59999));
    assert(reader.poll(client, 60000));
    assert(reader.result().failure == FailureKind::NoResponse);
    std::cout << "HTTP publish status reader tests passed\n";
}
