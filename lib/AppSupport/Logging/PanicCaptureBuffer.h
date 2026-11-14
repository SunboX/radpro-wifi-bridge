// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stddef.h>
#include <stdint.h>
namespace PanicCaptureBuffer {
constexpr uint32_t kMagic = 0x50414e31;
constexpr size_t kCapacity = 3072;
struct State {
    uint32_t magic, length, lengthInverse, hash, truncated, bootId;
    char elfSha256[65];
    char text[kCapacity];
};
inline void reset(volatile State &state) {
    state.magic = 0;
    state.length = 0;
    state.lengthInverse = ~uint32_t(0);
    state.hash = 2166136261u;
    state.truncated = 0;
    state.bootId = 0;
    state.elfSha256[0] = 0;
    state.text[0] = 0;
    state.magic = kMagic;
}
inline bool headerValid(const volatile State &state) {
    return state.magic == kMagic && state.length <= kCapacity - 1 &&
           state.lengthInverse == ~state.length;
}
#if defined(ESP_PLATFORM)
#define PANIC_ALWAYS_INLINE __attribute__((always_inline)) inline
#else
#define PANIC_ALWAYS_INLINE inline
#endif
PANIC_ALWAYS_INLINE void append(volatile State &state, char c) {
    // Panic printing is serialized by IDF after the other core is stalled.
    if (state.magic != kMagic || state.length >= kCapacity) return;
    const uint32_t length = state.length;
    if (length == kCapacity - 1) { state.truncated = 1; return; }
    state.text[length] = c;
    state.text[length + 1] = 0;
    state.hash = (state.hash ^ static_cast<unsigned char>(c)) * 16777619u;
    state.lengthInverse = ~(length + 1);
    state.length = length + 1;
}
inline bool valid(const volatile State &state) {
    if (!headerValid(state)) return false;
    uint32_t hash = 2166136261u;
    for (uint32_t n = 0; n < state.length; ++n)
        hash = (hash ^ static_cast<unsigned char>(state.text[n])) * 16777619u;
    return hash == state.hash && state.text[state.length] == 0;
}
#undef PANIC_ALWAYS_INLINE
}
