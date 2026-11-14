// SPDX-License-Identifier: GPL-3.0-or-later
#include "Safecast/SafecastSampleBuffer.h"
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <new>
static bool rejectAllocations = false;
void *operator new(std::size_t n) {
    if (rejectAllocations) throw std::bad_alloc();
    if (void *p = std::malloc(n)) return p;
    throw std::bad_alloc();
}
void operator delete(void *p) noexcept { std::free(p); }
int main() {
    SafecastSampleBuffer b;
    rejectAllocations = true; // Every operation below must work even with no allocatable heap.
    b.configure(false, false, 300000);
    for (uint32_t n = 0; n < 1000000; ++n) {
        b.observe(false, 25, n * 1000);
        b.observe(true, .25f, n * 1000);
    }
    assert(b.snapshot().count == 0);
    float value = 0;
    assert(b.latest(false, value) && value == 25);
    assert(b.latest(true, value) && value == .25f);
    b.configure(true, false, 300000);
    // A full simulated 48 hours, including ordinary rolling-window expiration.
    for (uint32_t n = 0; n < 172800; ++n) {
        b.observe(false, 40, n * 1000);
        b.observe(true, .4f, n * 1000);
        assert(b.snapshot().count <= 301);
        assert(b.average(n * 1000, value) && value == 40);
    }
    assert(b.snapshot().capacityDrops == 0);
    b.configure(false, false, 300000);
    assert(b.snapshot().count == 0); // Disabling releases history immediately, keeps latest.
    assert(b.latest(true, value) && value == .4f);
    b.configure(true, true, 300000);
    b.observe(false, 999, 0);
    assert(!b.average(0, value)); // Only the selected unit has averaging history.
    for (uint32_t n = 0; n < 4096; ++n) b.observe(true, static_cast<float>(n), n);
    assert(b.snapshot().count == 1024 && b.snapshot().capacityDrops == 3072);
    assert(b.average(4095, value) && std::fabs(value - (3072.f + 4095.f) / 2) < .001f);
    assert(!b.average(304096, value));
    assert(b.latest(true, value) && value == 4095); // Latest-only test upload survives expiry.
    b.configure(true, false, 100);
    b.observe(false, 10, UINT32_MAX - 49);
    b.observe(false, 30, 20);
    assert(b.average(20, value) && value == 20);
    assert(b.average(51, value) && value == 30);
    b.configure(true, false, 200); // Interval changes cannot reuse an incompatible average.
    assert(!b.average(52, value));
    b.observe(false, 7, 52);
    b.clear();
    assert(!b.latest(false, value) && !b.latest(true, value) && !b.average(52, value));
}
