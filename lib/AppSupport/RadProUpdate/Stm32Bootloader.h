// SPDX-FileCopyrightText: 2026 André Fiedler
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace RadProUpdate
{
// STM32 USART ROM protocol (AN3155), also used by the upstream installer.
class Transport
{
public:
    virtual ~Transport() = default;
    virtual bool write(const uint8_t *data, size_t size) = 0;
    virtual bool read(uint8_t *data, size_t size, uint32_t timeoutMs) = 0;
    virtual void drain() = 0;
};

class Stm32Bootloader
{
public:
    explicit Stm32Bootloader(Transport &transport) : io_(transport) {}
    bool sync()
    {
        // A second sync byte can return NACK when the ROM is already synced.
        for (int i = 0; i < 3; ++i)
        {
            io_.drain();
            const uint8_t syncByte = 0x7f;
            uint8_t reply = 0;
            if (io_.write(&syncByte, 1) && io_.read(&reply, 1, 500) &&
                (reply == 0x79 || reply == 0x1f))
                return true;
        }
        return false;
    }
    bool identify(uint16_t expected)
    {
        uint8_t reply[4]{};
        return command(0x02) && io_.read(reply, 1, 1000) && reply[0] == 1 &&
               io_.read(reply + 1, 3, 1000) && reply[3] == 0x79 &&
               ((reply[1] << 8) | reply[2]) == expected;
    }
    bool erasePage(uint16_t page, bool extended)
    {
        if (!extended && page > 255)
            return false;
        uint8_t packet[5] = {0, 0, static_cast<uint8_t>(page >> 8), static_cast<uint8_t>(page), 0};
        if (!extended)
            packet[1] = static_cast<uint8_t>(page);
        return command(extended ? 0x44 : 0x43) && checksumPacket(packet, extended ? 4 : 2, 4000);
    }
    bool writeBlock(uint32_t address, const uint8_t *data, size_t size)
    {
        if (!size || size > 256 || size % 4 || address % 4)
            return false;
        uint8_t packet[258];
        packet[0] = static_cast<uint8_t>(size - 1);
        std::memcpy(packet + 1, data, size);
        return command(0x31) && sendAddress(address) && checksumPacket(packet, size + 1, 2000);
    }
    bool readBlock(uint32_t address, uint8_t *data, size_t size)
    {
        if (!size || size > 256)
            return false;
        return command(0x11) && sendAddress(address) && command(static_cast<uint8_t>(size - 1)) &&
               io_.read(data, size, 2000);
    }
    bool go(uint32_t address) { return command(0x21) && sendAddress(address); }

private:
    bool ack(uint32_t timeout = 1000)
    {
        uint8_t value = 0;
        return io_.read(&value, 1, timeout) && value == 0x79;
    }
    bool command(uint8_t value)
    {
        const uint8_t packet[] = {value, static_cast<uint8_t>(value ^ 0xff)};
        return io_.write(packet, sizeof(packet)) && ack();
    }
    bool checksumPacket(uint8_t *packet, size_t size, uint32_t timeout)
    {
        uint8_t checksum = 0;
        for (size_t i = 0; i < size; ++i)
            checksum ^= packet[i];
        packet[size] = checksum;
        return io_.write(packet, size + 1) && ack(timeout);
    }
    bool sendAddress(uint32_t address)
    {
        uint8_t packet[5] = {static_cast<uint8_t>(address >> 24), static_cast<uint8_t>(address >> 16),
                             static_cast<uint8_t>(address >> 8), static_cast<uint8_t>(address), 0};
        return checksumPacket(packet, 4, 1000);
    }
    Transport &io_;
};
}
