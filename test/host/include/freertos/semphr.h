/*
 * SPDX-FileCopyrightText: 2026 André Fiedler
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "freertos/FreeRTOS.h"

using SemaphoreHandle_t = void *;

inline SemaphoreHandle_t xSemaphoreCreateMutex()
{
    return reinterpret_cast<SemaphoreHandle_t>(0x1);
}

inline int xSemaphoreTake(SemaphoreHandle_t, TickType_t)
{
    return pdTRUE;
}

inline void xSemaphoreGive(SemaphoreHandle_t)
{
}
