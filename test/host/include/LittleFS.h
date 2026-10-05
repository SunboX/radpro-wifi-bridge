// SPDX-FileCopyrightText: 2026 André Fiedler
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "Arduino.h"

struct File
{
    explicit operator bool() const { return false; }
    String readString() { return {}; }
    void close() {}
};

struct LittleFSClass
{
    File open(const char *, const char *) { return {}; }
};

inline LittleFSClass LittleFS;
