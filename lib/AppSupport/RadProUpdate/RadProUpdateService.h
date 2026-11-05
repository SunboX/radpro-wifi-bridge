// SPDX-FileCopyrightText: 2026 André Fiedler
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <Arduino.h>
#include <LittleFS.h>
#include <atomic>
#include "DeviceManager.h"
#include "DeviceInfo/DeviceInfoStore.h"
#include "FirmwareCatalog.h"

class RadProUpdateService
{
public:
    RadProUpdateService(UsbCdcHost &usb, DeviceManager &manager, DeviceInfoStore &info);
    bool beginUpload(const String &locale, String &error);
    bool uploadChunk(size_t offset, const uint8_t *data, size_t size, String &error);
    bool start(String &error);
    bool cancel(String &error);
    void poll();
    bool busy() const { return busy_.load(); }
    bool ownsUsb() const { return busy_ || recoveryRequired_; }
    bool blocksBridgeUpdate() const
    { return ownsUsb() || strcmp(phase_.load(), "uploading") == 0 || strcmp(phase_.load(), "staged") == 0; }
    String statusJson() const;

private:
    static void task(void *arg);
    void run();
    bool flash(String &error);
    bool readLine(String &line, uint32_t timeout);
    bool queryIdentity(String &error);
    bool sameConnection() const;
    bool validateImage(String &error);
    UsbCdcHost &usb_;
    DeviceManager &manager_;
    DeviceInfoStore &info_;
    const RadProUpdate::Model *model_ = nullptr;
    const RadProUpdate::FirmwareImage *image_ = nullptr;
    String deviceId_;
    uint32_t generation_ = 0;
    File upload_;
    size_t uploaded_ = 0;
    unsigned long lastUploadMs_ = 0;
    bool resumeManager_ = false;
    String error_;
    std::atomic<bool> busy_{false};
    std::atomic<bool> done_{false};
    std::atomic<bool> recoveryRequired_{false};
    std::atomic<const char *> phase_{"idle"};
    std::atomic<size_t> progress_{0};
};
