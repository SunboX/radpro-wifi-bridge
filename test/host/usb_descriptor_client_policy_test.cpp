// SPDX-FileCopyrightText: 2026 André Fiedler
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cassert>
#include <iostream>

#include "UsbDescriptorClientPolicy.h"

#ifndef EXPECT_USB_DESCRIPTOR_CLIENT_ENABLED
#error "EXPECT_USB_DESCRIPTOR_CLIENT_ENABLED must be set"
#endif

int main()
{
    assert(UsbDescriptorClientPolicy::isEnabled() ==
           static_cast<bool>(EXPECT_USB_DESCRIPTOR_CLIENT_ENABLED));
    std::cout << "usb descriptor client policy tests passed\n";
}
