#pragma once

#include <Arduino.h>
#include <vector>
#include <WiFiClient.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

class DeviceManager;
class UsbCdcHost;
class MqttPublisher;
class OpenSenseMapPublisher;
class GmcMapPublisher;
class RadmonPublisher;

class OtaUpdateService
{
public:
    struct Status
    {
        bool busy = false;
        bool needsReboot = false;
        String lastError;
        size_t partsCompleted = 0;
        size_t partsTotal = 0;
        String targetVersion;
    };

    bool begin(const String &manifestJson);
    bool beginPart(const String &path, uint32_t offset, size_t size);
    bool writePartChunk(const uint8_t *data, size_t len);
    bool finalizePart();
    bool finish();
    void reset();
    void abort(const String &message);
    Status status() const;

    static void EnterUpdateMode(DeviceManager &deviceManager,
                                UsbCdcHost &usbHost,
                                MqttPublisher &mqtt,
                                OpenSenseMapPublisher &osem,
                                GmcMapPublisher &gmc,
                                RadmonPublisher &radmon,
                                bool &updateFlag);

private:
    struct PartInfo
    {
        String path;
        uint32_t offset = 0;
        bool received = false;
        bool isFirmware = false;
        bool skip = false;
    };

    struct ActivePart
    {
        PartInfo *info = nullptr;
        uint32_t offset = 0;
        size_t expectedSize = 0;
        size_t written = 0;
        bool isOta = false;
        esp_ota_handle_t otaHandle = 0;
        const esp_partition_t *partition = nullptr;
        bool skip = false;
    };

    bool eraseRegion(uint32_t offset, size_t size);
    bool ensureFsUnmounted(uint32_t offset, const String &path);
    bool isProtectedRegion(uint32_t offset, const String &path) const;

    std::vector<PartInfo> parts_;
    bool busy_ = false;
    bool needsReboot_ = false;
    bool fsUnmounted_ = false;
    String lastError_;
    String targetVersion_;
    ActivePart active_;
    const esp_partition_t *targetOtaPartition_ = nullptr;
};
