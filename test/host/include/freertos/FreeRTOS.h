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

using portMUX_TYPE = int;
constexpr portMUX_TYPE portMUX_INITIALIZER_UNLOCKED = 0;
inline void portENTER_CRITICAL(portMUX_TYPE *) {}
inline void portEXIT_CRITICAL(portMUX_TYPE *) {}
