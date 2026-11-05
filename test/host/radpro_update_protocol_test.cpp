// SPDX-FileCopyrightText: 2026 André Fiedler
// SPDX-License-Identifier: GPL-3.0-or-later
#include "RadProUpdate/Stm32Bootloader.h"
#include "RadProUpdate/FirmwareCatalog.h"
#include <cassert>
#include <deque>
#include <vector>
#include <iostream>

struct Fake : RadProUpdate::Transport
{
    std::vector<std::vector<uint8_t>> sent;
    std::deque<uint8_t> replies;
    bool writeOk = true;
    bool write(const uint8_t *data, size_t size) override
    { sent.emplace_back(data, data + size); return writeOk; }
    bool read(uint8_t *data, size_t size, uint32_t) override
    {
        if (replies.size() < size) return false;
        for (size_t i = 0; i < size; ++i) { data[i] = replies.front(); replies.pop_front(); }
        return true;
    }
    void drain() override {}
};
int main()
{
    using namespace RadProUpdate;
    assert(findModel("FNIRSI GC-01") == nullptr);
    assert(findModel("FNIRSI GC-03") == nullptr);
    assert(findModel("FS2011 (STM32F051C8)") == nullptr);
    assert(!supportsFirmware("Rad Pro 3.0.1"));
    assert(!supportsFirmware("Rad Pro 3.1test17"));
    assert(!supportsFirmware("Rad Pro 4.0.0"));
    assert(!supportsFirmware("Rad Pro 3.1.1 garbage"));
    assert(supportsFirmware("Rad Pro 3.0.2"));
    assert(supportsFirmware("Rad Pro 3.1.1"));
    for (const auto &model : models)
    {
        const auto *image = findImage(model, "de");
        assert(image && image->size < model.flashSize - 2048);
        assert(image->size % 4 == 0 && std::strlen(image->sha256) == 64);
        assert(!findImage(model, "../en"));
        Fake io;
        Stm32Bootloader rom(io);
        io.replies = {0x79, 1, uint8_t(model.productId >> 8), uint8_t(model.productId), 0x79};
        assert(rom.identify(model.productId));
        assert(io.sent[0] == std::vector<uint8_t>({2, 0xfd}));
    }
    {
        Fake io; Stm32Bootloader rom(io);
        io.replies = {0x79, 1, 4, 0x35, 0x79};
        assert(!rom.identify(0x460)); // Wrong chip: identification fails without an erase.
        assert(io.sent.size() == 1);
    }
    {
        Fake io; Stm32Bootloader rom(io);
        io.replies = {0x79, 0x79};
        assert(rom.erasePage(0x12, true));
        assert(io.sent[0] == std::vector<uint8_t>({0x44, 0xbb}));
        assert(io.sent[1] == std::vector<uint8_t>({0, 0, 0, 0x12, 0x12}));
    }
    {
        Fake io; Stm32Bootloader rom(io);
        io.replies = {0x79, 0x79};
        assert(rom.erasePage(0x12, false));
        assert(io.sent[0] == std::vector<uint8_t>({0x43, 0xbc}));
        assert(io.sent[1] == std::vector<uint8_t>({0, 0x12, 0x12}));
        assert(!rom.erasePage(256, false));
    }
    {
        Fake io; Stm32Bootloader rom(io);
        uint8_t data[] = {1, 2, 3, 4};
        io.replies = {0x79, 0x79, 0x79};
        assert(rom.writeBlock(0x08000100, data, 4));
        assert(io.sent[1] == std::vector<uint8_t>({8, 0, 1, 0, 9}));
        assert(io.sent[2] == std::vector<uint8_t>({3, 1, 2, 3, 4, 7}));
        assert(!rom.writeBlock(0x08000001, data, 4));
        assert(!rom.writeBlock(0x08000000, data, 3));
        assert(!rom.writeBlock(0x08000000, data, 0));
        io.replies = {0x1f};
        assert(!rom.writeBlock(0x08000000, data, 4)); // No payload after NACK.
        assert(io.sent.size() == 4);
    }
    {
        Fake io; Stm32Bootloader rom(io);
        io.replies = {0x79, 0x79, 0x79, 1, 2, 3, 4};
        uint8_t data[4]{};
        assert(rom.readBlock(0x08000000, data, 4));
        assert(data[0] == 1 && data[3] == 4);
        assert(io.sent[2] == std::vector<uint8_t>({3, 0xfc}));
        assert(!rom.go(0x08000000)); // Missing ACK fails.
    }
    std::cout << "RadPro update policy and ROM protocol checks passed\n";
}
