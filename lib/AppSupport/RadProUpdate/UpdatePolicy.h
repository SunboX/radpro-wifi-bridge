// SPDX-FileCopyrightText: 2026 André Fiedler
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace RadProUpdate
{
struct Model
{
    const char *name;
    const char *environment;
    uint16_t productId;
    uint32_t flashSize;
    bool extendedErase;
};
static constexpr Model models[] = {
    {"Bosean FS-600", "bosean-fs600", 0x460, 0x20000, true},
    {"Bosean FS-1000", "bosean-fs1000", 0x460, 0x20000, true},
    {"Bosean FS-5000", "bosean-fs5000", 0x435, 0x40000, true},
    {"GQ GMC-800", "gq-gmc800", 0x414, 0x40000, false},
};
inline const Model *findModel(const char *name)
{
    for (const auto &model : models)
        if (std::strcmp(name, model.name) == 0)
            return &model;
    return nullptr;
}
inline bool supportsFirmware(const char *firmware)
{
    // Conservative floor: the release explicitly documents 3.0.2+ for Bosean.
    // Reject test builds, unknown firmware and future incompatible major versions.
    unsigned major = 0, minor = 0, patch = 0;
    int consumed = 0;
    if (std::sscanf(firmware, "Rad Pro %u.%u.%u%n", &major, &minor, &patch, &consumed) != 3 ||
        firmware[consumed] != '\0')
        return false;
    return major == 3 && (minor > 0 || patch >= 2);
}
struct FirmwareImage
{
    const char *environment;
    const char *locale;
    size_t size;
    const char *sha256;
};
}
