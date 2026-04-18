#pragma once

#include <Arduino.h>
#include <cstdio>

namespace UsbDiagnosticMessages
{
inline String formatObservedDevice(uint8_t address, uint16_t vid, uint16_t pid, uint8_t deviceClass)
{
    char buffer[96];
    std::snprintf(buffer,
                  sizeof(buffer),
                  "USB diag: observed addr=%u VID=0x%04X PID=0x%04X class=0x%02X",
                  static_cast<unsigned>(address),
                  static_cast<unsigned>(vid),
                  static_cast<unsigned>(pid),
                  static_cast<unsigned>(deviceClass));
    return String(buffer);
}

inline String formatInterfaceDescriptor(uint8_t interfaceNumber,
                                        uint8_t interfaceClass,
                                        uint8_t subClass,
                                        uint8_t protocol)
{
    char buffer[96];
    std::snprintf(buffer,
                  sizeof(buffer),
                  "USB diag: IF#%u class=0x%02X sub=0x%02X proto=0x%02X",
                  static_cast<unsigned>(interfaceNumber),
                  static_cast<unsigned>(interfaceClass),
                  static_cast<unsigned>(subClass),
                  static_cast<unsigned>(protocol));
    return String(buffer);
}

inline String formatEndpointDescriptor(uint8_t endpointAddress,
                                       uint8_t attributes,
                                       uint16_t maxPacketSize,
                                       uint8_t interval)
{
    char buffer[112];
    std::snprintf(buffer,
                  sizeof(buffer),
                  "USB diag: EP addr=0x%02X attr=0x%02X maxPkt=%u interval=%u",
                  static_cast<unsigned>(endpointAddress),
                  static_cast<unsigned>(attributes),
                  static_cast<unsigned>(maxPacketSize),
                  static_cast<unsigned>(interval));
    return String(buffer);
}

inline String formatOpenFailureSummary(uint16_t vid,
                                       uint16_t pid,
                                       uint8_t deviceClass,
                                       bool allowlisted,
                                       uint8_t firstInterface,
                                       uint8_t lastInterface)
{
    char buffer[128];
    std::snprintf(buffer,
                  sizeof(buffer),
                  "USB diag: open failed for VID=0x%04X PID=0x%04X class=0x%02X allowlisted=%s ifaces=%u-%u",
                  static_cast<unsigned>(vid),
                  static_cast<unsigned>(pid),
                  static_cast<unsigned>(deviceClass),
                  allowlisted ? "yes" : "no",
                  static_cast<unsigned>(firstInterface),
                  static_cast<unsigned>(lastInterface));
    return String(buffer);
}

inline String formatOpenSuccess(const char *transport, uint8_t interfaceIndex, uint16_t vid, uint16_t pid)
{
    char buffer[112];
    std::snprintf(buffer,
                  sizeof(buffer),
                  "USB diag: %s open OK iface=%u VID=0x%04X PID=0x%04X",
                  transport ? transport : "CDC",
                  static_cast<unsigned>(interfaceIndex),
                  static_cast<unsigned>(vid),
                  static_cast<unsigned>(pid));
    return String(buffer);
}
} // namespace UsbDiagnosticMessages
