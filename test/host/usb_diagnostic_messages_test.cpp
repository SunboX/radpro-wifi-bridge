// SPDX-FileCopyrightText: 2026 André Fiedler
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cassert>
#include <iostream>

#include "UsbDiagnosticMessages.h"

namespace
{
void testFormatsObservedDeviceSummary()
{
    const String line = UsbDiagnosticMessages::formatObservedDevice(4, 0x1A86, 0x55D4, 0xFF);
    assert(line == "USB diag: observed addr=4 VID=0x1A86 PID=0x55D4 class=0xFF");
}

void testFormatsOpenFailureSummary()
{
    const String line = UsbDiagnosticMessages::formatOpenFailureSummary(0x1A86, 0x55D4, 0xFF, true, 0, 2);
    assert(line == "USB diag: open failed for VID=0x1A86 PID=0x55D4 class=0xFF allowlisted=yes ifaces=0-2");
}

void testFormatsOpenSuccess()
{
    const String line = UsbDiagnosticMessages::formatOpenSuccess("VCP", 1, 0x1A86, 0x55D4);
    assert(line == "USB diag: VCP open OK iface=1 VID=0x1A86 PID=0x55D4");
}
} // namespace

int main()
{
    testFormatsObservedDeviceSummary();
    testFormatsOpenFailureSummary();
    testFormatsOpenSuccess();
    std::cout << "usb diagnostic message tests passed\n";
    return 0;
}
