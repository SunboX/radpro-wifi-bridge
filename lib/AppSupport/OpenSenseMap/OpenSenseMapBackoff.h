/*
 * SPDX-FileCopyrightText: 2026 André Fiedler
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

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
