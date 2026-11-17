// SPDX-FileCopyrightText: 2026 André Fiedler
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cassert>
#include <iostream>
#include <string>
#include <thread>
#include "Network/DnsRequest.h"

int main()
{
    BridgeDns::Request request;
    uint32_t result = 0;
    assert(!request.start(nullptr));
    assert(!request.start(""));
    assert(!request.start(std::string(256, 'x').c_str()));
    assert(request.start("first.example"));
    assert(request.workerStart());
    assert(!request.start("second.example"));
    assert(request.workerFinish(true, 0x01020304));
    assert(request.complete());
    assert(request.take(result) && result == 0x01020304 && request.idle());

    // The caller has returned, but the worker still owns the original name.
    assert(request.start("slow.example"));
    assert(request.workerStart());
    request.abandon();
    assert(!request.start("new.example"));
    assert(std::string(request.name()) == "slow.example");
    assert(!request.workerFinish(true, 0xdeadbeef));
    assert(request.idle());
    assert(request.start("new.example"));
    assert(request.workerStart());
    assert(request.workerFinish(true, 123));
    assert(request.take(result) && result == 123); // Never receives the abandoned answer.

    assert(request.start("queued.example"));
    request.abandon();
    assert(!request.workerStart() && request.idle());
    assert(request.start("failed.example"));
    assert(request.workerStart());
    assert(request.workerFinish(false, 123));
    assert(!request.take(result) && result == 0);

    // Exercise completion racing a caller deadline using the real state class.
    for (unsigned i = 0; i < 2000; ++i)
    {
        assert(request.start("race.example"));
        assert(request.workerStart());
        std::thread worker([&]() { request.workerFinish(true, i); });
        request.abandon();
        worker.join();
        assert(request.idle());
    }
    std::cout << "DNS request lifetime tests passed\n";
}
