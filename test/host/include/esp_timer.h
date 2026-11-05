// SPDX-FileCopyrightText: 2026 André Fiedler
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <cstdint>

inline int64_t g_test_uptime_us = 0;
inline int64_t esp_timer_get_time() { return g_test_uptime_us; }
