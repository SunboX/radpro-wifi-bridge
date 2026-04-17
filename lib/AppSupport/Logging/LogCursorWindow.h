#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace LogCursorWindow
{
struct Selection
{
    size_t startIndex = 0;
    size_t returnedCount = 0;
    uint32_t oldestId = 0;
    uint32_t latestId = 0;
    bool reset = false;
};

template <typename Entry>
Selection select(const std::vector<Entry> &entries, uint32_t afterId)
{
    Selection selection;
    if (entries.empty())
        return selection;

    selection.oldestId = entries.front().id;
    selection.latestId = entries.back().id;

    if (afterId == 0)
    {
        selection.returnedCount = entries.size();
        return selection;
    }

    if (afterId < selection.oldestId && (selection.oldestId - afterId) > 1)
    {
        selection.reset = true;
        selection.returnedCount = entries.size();
        return selection;
    }

    selection.startIndex = entries.size();
    for (size_t i = 0; i < entries.size(); ++i)
    {
        if (entries[i].id > afterId)
        {
            selection.startIndex = i;
            break;
        }
    }
    selection.returnedCount = entries.size() - selection.startIndex;
    return selection;
}
} // namespace LogCursorWindow
