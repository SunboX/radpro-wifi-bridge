// SPDX-FileCopyrightText: 2026 André Fiedler
// SPDX-License-Identifier: GPL-3.0-or-later
#include <cassert>
#include <cstdint>
#include <string>
#include "Http/HttpHeaderReader.h"

struct Client {
    std::string data;
    uint32_t &now;
    uint32_t interval = 0;
    size_t pos = 0;
    int read() {
        if (pos == data.size() || now < pos * interval) return -1;
        return static_cast<unsigned char>(data[pos++]);
    }
};

int main() {
    using RadProHttp::HeaderReader;
    uint32_t now = 0;
    unsigned waits = 0;
    auto clock = [&]() { return now; };
    auto idle = [&]() { ++now; ++waits; };
    std::string line;
    Client normal{"POST /backup/restore HTTP/1.1\r\nContent-Length: 4\r\n\r\nBODY", now};
    HeaderReader reader(now);
    assert(reader.readLine(normal, line, clock, idle) && line == "POST /backup/restore HTTP/1.1");
    assert(reader.readLine(normal, line, clock, idle) && line == "Content-Length: 4");
    assert(reader.readLine(normal, line, clock, idle) && line.empty());
    assert(normal.data.substr(normal.pos) == "BODY"); // Deadline/budget never consume the upload body.

    now = 0;
    waits = 0;
    Client slow{"X-Slow: aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\r\n", now, 350};
    HeaderReader slowReader(now);
    assert(!slowReader.readLine(slow, line, clock, idle));
    assert(now == HeaderReader::kTimeoutMs && waits > 0 && slow.pos < slow.data.size());

    now = 0;
    Client split{"one\r\ntwo\r\n", now, 350};
    HeaderReader shared(now);
    assert(shared.readLine(split, line, clock, idle));
    assert(!shared.readLine(split, line, clock, idle)); // The deadline isn't reset per line/byte.

    now = 0;
    Client huge{std::string(HeaderReader::kMaxBytes + 20, 'a'), now};
    HeaderReader bounded(now);
    assert(!bounded.readLine(huge, line, clock, idle));
    assert(huge.pos == HeaderReader::kMaxBytes && now > 0);
    for (auto text : {"bad\n", "bad\rX", "bad\r", "unfinished"}) {
        now = 0;
        Client incomplete{text, now};
        HeaderReader malformed(now);
        assert(!malformed.readLine(incomplete, line, clock, idle));
    }
    now = UINT32_MAX - 100;
    Client silent{"", now};
    HeaderReader wrapping(now);
    assert(!wrapping.readLine(silent, line, clock, idle));
    assert(now == static_cast<uint32_t>(UINT32_MAX - 100 + HeaderReader::kTimeoutMs));
}
