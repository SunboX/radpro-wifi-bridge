/*
 * SPDX-FileCopyrightText: 2026 André Fiedler
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>

using TickType_t = uint32_t;

constexpr TickType_t portMAX_DELAY = 0xffffffffu;
constexpr int pdTRUE = 1;
constexpr int pdFALSE = 0;
