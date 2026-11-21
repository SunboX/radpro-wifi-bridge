// SPDX-FileCopyrightText: 2026 André Fiedler
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <cstdint>

namespace BridgeDnsResolver
{
struct Snapshot
{
    bool ready;
    bool pending;
    uint32_t lookups;
    uint32_t successes;
    uint32_t failures;
    uint32_t timeouts;
    uint32_t busy;
    uint32_t lateCompletions;
};
// The optional callback runs only on the setup task, while it awaits DNS.
// It must not start another lookup or call into the web portal.
bool begin(void (*serviceWhileWaiting)() = nullptr);
Snapshot snapshot();
}

extern "C" int radpro_resolve_hostname(const char *hostname, uint32_t *address);
