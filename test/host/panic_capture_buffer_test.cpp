// SPDX-License-Identifier: GPL-3.0-or-later
#include <cassert>
#include <cstring>
#include "Logging/PanicCaptureBuffer.h"
int main() {
    using namespace PanicCaptureBuffer;
    State state{};
    assert(!valid(state));
    append(state, 'x');
    assert(!valid(state));
    reset(state);
    assert(valid(state) && state.length == 0);
    const char text[] = "abort() was called\nBacktrace: 0x42001234\n";
    for (char c : text) if (c) append(state, c);
    assert(valid(state) && std::strcmp(state.text, text) == 0);
    State warmReset = state;
    assert(valid(warmReset));
    warmReset.text[3] ^= 1;
    assert(!valid(warmReset));
    warmReset = state;
    warmReset.lengthInverse ^= 1;
    assert(!valid(warmReset));
    warmReset = state;
    warmReset.length = kCapacity + 10;
    assert(!valid(warmReset));
    reset(state);
    for (size_t n = 0; n < kCapacity + 100; ++n) append(state, 'a');
    assert(valid(state) && state.length == kCapacity - 1 && state.truncated);
    assert(state.text[kCapacity - 1] == 0);
    reset(state);
    assert(valid(state) && !state.truncated && state.length == 0);
}
