// SPDX-FileCopyrightText: 2026 André Fiedler
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cassert>
#include <iostream>
#include <vector>

#include "Logging/LogCursorWindow.h"

namespace
{
struct Entry
{
    uint32_t id;
    const char *text;
};

void testZeroCursorReturnsFullBuffer()
{
    const std::vector<Entry> entries = {
        {101, "alpha"},
        {102, "beta"},
        {103, "gamma"},
    };

    const auto window = LogCursorWindow::select(entries, 0);
    assert(window.reset == false);
    assert(window.startIndex == 0);
    assert(window.returnedCount == 3);
    assert(window.oldestId == 101);
    assert(window.latestId == 103);
}

void testCursorReturnsOnlyNewEntries()
{
    const std::vector<Entry> entries = {
        {101, "alpha"},
        {102, "beta"},
        {103, "gamma"},
    };

    const auto window = LogCursorWindow::select(entries, 102);
    assert(window.reset == false);
    assert(window.startIndex == 2);
    assert(window.returnedCount == 1);
    assert(window.oldestId == 101);
    assert(window.latestId == 103);
}

void testStaleCursorRequestsReset()
{
    const std::vector<Entry> entries = {
        {201, "alpha"},
        {202, "beta"},
        {203, "gamma"},
    };

    const auto window = LogCursorWindow::select(entries, 150);
    assert(window.reset);
    assert(window.startIndex == 0);
    assert(window.returnedCount == 3);
    assert(window.oldestId == 201);
    assert(window.latestId == 203);
}
} // namespace

int main()
{
    testZeroCursorReturnsFullBuffer();
    testCursorReturnsOnlyNewEntries();
    testStaleCursorRequestsReset();
    std::cout << "log cursor window tests passed\n";
    return 0;
}
