/*
 * SPDX-FileCopyrightText: 2026 André Fiedler
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#ifndef RADPRO_USB_DESCRIPTOR_CLIENT_ENABLED
#define RADPRO_USB_DESCRIPTOR_CLIENT_ENABLED 1
#endif

namespace UsbDescriptorClientPolicy
{
constexpr bool isEnabled()
{
    return RADPRO_USB_DESCRIPTOR_CLIENT_ENABLED != 0;
}
} // namespace UsbDescriptorClientPolicy
