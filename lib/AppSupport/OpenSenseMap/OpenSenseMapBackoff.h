#pragma once

namespace OpenSenseMapBackoff
{
inline unsigned long preserveActiveSuppression(unsigned long now, unsigned long suppressUntilMs)
{
    if (suppressUntilMs != 0 && static_cast<long>(now - suppressUntilMs) < 0)
        return suppressUntilMs;
    return 0;
}
} // namespace OpenSenseMapBackoff
