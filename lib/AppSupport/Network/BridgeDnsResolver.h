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
// Call once during setup, before starting Wi-Fi services.
bool begin();
Snapshot snapshot();
}

extern "C" int radpro_resolve_hostname(const char *hostname, uint32_t *address);
