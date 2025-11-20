#include "Ota/OtaUpdateService.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <esp_flash.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

namespace
{
    constexpr uint32_t kLittleFsOffset = 0x00E00000;
}

bool OtaUpdateService::begin(const String &manifestJson)
{
    if (busy_)
    {
        lastError_ = F("OTA already running.");
        return false;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, manifestJson);
    if (err)
    {
        lastError_ = String(F("Manifest parse failed: ")) + err.c_str();
        return false;
    }

    JsonArray builds = doc["builds"].as<JsonArray>();
    if (!builds || builds.size() == 0)
    {
        lastError_ = F("Manifest missing builds.");
        return false;
    }

    JsonArray parts = builds[0]["parts"].as<JsonArray>();
    if (!parts || parts.size() == 0)
    {
        lastError_ = F("Manifest missing parts.");
        return false;
    }

    parts_.clear();
    parts_.reserve(parts.size());
    size_t skipped = 0;
    for (const JsonVariantConst &entry : parts)
    {
        PartInfo info;
        info.path = entry["path"].as<const char *>();
        info.offset = entry["offset"] | 0;
        if (!info.path.length())
        {
            lastError_ = F("Manifest part missing path.");
            parts_.clear();
            return false;
        }
        String lower = info.path;
        lower.toLowerCase();
        info.isFirmware = lower.indexOf(F("bridge.bin")) >= 0 || lower.indexOf(F("wifi-bridge.bin")) >= 0;
        if (isProtectedRegion(info.offset, info.path))
        {
            info.skip = true;
            ++skipped;
        }
        parts_.push_back(info);
    }

    if (parts_.empty())
    {
        lastError_ = String(F("Manifest parts are not writable (skipped: "));
        lastError_ += skipped;
        lastError_ += F(").");
        return false;
    }

    targetVersion_ = doc["version"].as<const char *>();
    busy_ = true;
    needsReboot_ = false;
    fsUnmounted_ = false;
    active_ = ActivePart{};
    lastError_ = String();
    targetOtaPartition_ = nullptr;
    return true;
}

bool OtaUpdateService::beginPart(const String &path, uint32_t offset, size_t size)
{
    if (!busy_)
    {
        lastError_ = F("OTA not started.");
        return false;
    }

    if (active_.info)
    {
        lastError_ = F("Another part is being written.");
        return false;
    }

    PartInfo *target = nullptr;
    for (auto &part : parts_)
    {
        if (!part.received && part.path == path)
        {
            target = &part;
            break;
        }
    }

    if (!target)
    {
        lastError_ = F("Unexpected part.");
        return false;
    }

    active_ = ActivePart{};
    active_.info = target;
    active_.expectedSize = size;
    active_.skip = target->skip;

    if (target->skip)
    {
        active_.skip = true;
        active_.info = target;
        active_.expectedSize = size;
        active_.written = 0;
        return true;
    }

    if (target->isFirmware)
    {
        if (!targetOtaPartition_)
            targetOtaPartition_ = esp_ota_get_next_update_partition(nullptr);
        if (!targetOtaPartition_)
        {
            lastError_ = F("No OTA partition available.");
            active_ = ActivePart{};
            return false;
        }
        if (size > targetOtaPartition_->size)
        {
            lastError_ = F("Firmware image too large for OTA partition.");
            active_ = ActivePart{};
            return false;
        }
        esp_err_t err = esp_ota_begin(targetOtaPartition_, size, &active_.otaHandle);
        if (err != ESP_OK)
        {
            lastError_ = String(F("esp_ota_begin failed: ")) + esp_err_to_name(err);
            active_ = ActivePart{};
            return false;
        }
        active_.isOta = true;
        active_.partition = targetOtaPartition_;
        active_.offset = targetOtaPartition_->address;
    }
    else
    {
        // Data partition (e.g., LittleFS). Use the partition table entry instead of manifest offset.
        const esp_partition_t *part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                                               ESP_PARTITION_SUBTYPE_DATA_SPIFFS,
                                                               "spiffs");
        if (!part)
        {
            lastError_ = F("LittleFS partition not found.");
            active_ = ActivePart{};
            return false;
        }
        if (size > part->size)
        {
            lastError_ = F("LittleFS image too large for partition.");
            active_ = ActivePart{};
            return false;
        }

        if (!ensureFsUnmounted(part->address, path))
        {
            active_ = ActivePart{};
            return false;
        }

        // Erase target range before writing.
        if (!eraseRegion(part->address, size))
        {
            active_ = ActivePart{};
            return false;
        }

        active_.offset = part->address;
        active_.partition = part;
    }

    active_.written = 0;
    return true;
}

bool OtaUpdateService::writePartChunk(const uint8_t *data, size_t len)
{
    if (!active_.info)
    {
        lastError_ = F("No active part.");
        return false;
    }

    if (active_.skip)
        return true;

    if (active_.written + len > active_.expectedSize)
    {
        lastError_ = F("Chunk exceeds expected size.");
        return false;
    }

    esp_err_t err = ESP_OK;
    if (active_.isOta)
    {
        err = esp_ota_write(active_.otaHandle, data, len);
    }
    else if (active_.partition)
    {
        err = esp_partition_write(active_.partition, active_.written, data, len);
    }
    else
    {
        err = ESP_ERR_INVALID_STATE;
    }

    if (err != ESP_OK)
    {
        lastError_ = String(F("Flash write failed: ")) + esp_err_to_name(err);
        return false;
    }

    active_.written += len;
    return true;
}

bool OtaUpdateService::finalizePart()
{
    if (!active_.info)
    {
        lastError_ = F("No active part.");
        return false;
    }

    if (active_.skip)
    {
        active_.info->received = true;
        active_ = ActivePart{};
        return true;
    }

    if (active_.written != active_.expectedSize)
    {
        lastError_ = F("Part size mismatch.");
        return false;
    }

    if (active_.isOta)
    {
        esp_err_t err = esp_ota_end(active_.otaHandle);
        if (err != ESP_OK)
        {
            lastError_ = String(F("OTA finalize failed: ")) + esp_err_to_name(err);
            return false;
        }
    }

    active_.info->received = true;
    active_ = ActivePart{};
    return true;
}

bool OtaUpdateService::finish()
{
    if (!busy_)
    {
        lastError_ = F("OTA not started.");
        return false;
    }

    if (active_.info)
    {
        lastError_ = F("Part write still in progress.");
        return false;
    }

    for (const auto &part : parts_)
    {
        if (part.skip)
            continue;
        if (!part.received)
        {
            lastError_ = F("Missing part data.");
            return false;
        }
    }

    if (targetOtaPartition_)
    {
        esp_err_t err = esp_ota_set_boot_partition(targetOtaPartition_);
        if (err != ESP_OK)
        {
            lastError_ = String(F("Failed to set OTA boot partition: ")) + esp_err_to_name(err);
            return false;
        }
    }

    needsReboot_ = true;
    busy_ = false;
    return true;
}

void OtaUpdateService::reset()
{
    parts_.clear();
    busy_ = false;
    needsReboot_ = false;
    fsUnmounted_ = false;
    lastError_ = String();
    active_ = ActivePart{};
    targetVersion_ = String();
    targetOtaPartition_ = nullptr;
}

void OtaUpdateService::abort(const String &message)
{
    lastError_ = message;
    reset();
}

OtaUpdateService::Status OtaUpdateService::status() const
{
    Status s;
    s.busy = busy_;
    s.needsReboot = needsReboot_;
    s.lastError = lastError_;
    s.targetVersion = targetVersion_;
    s.partsTotal = parts_.size();
    size_t completed = 0;
    for (const auto &part : parts_)
    {
        if (part.received)
            ++completed;
    }
    s.partsCompleted = completed;
    return s;
}

bool OtaUpdateService::eraseRegion(uint32_t offset, size_t size)
{
    size_t alignedSize = (size + 0xFFF) & ~0xFFF;
    esp_err_t err = esp_flash_erase_region(nullptr, offset, alignedSize);
    if (err != ESP_OK)
    {
        lastError_ = String(F("Erase failed: ")) + esp_err_to_name(err);
        return false;
    }
    return true;
}

bool OtaUpdateService::ensureFsUnmounted(uint32_t offset, const String &path)
{
    if (fsUnmounted_)
        return true;

    if (offset >= kLittleFsOffset || path.indexOf(F("littlefs")) >= 0)
    {
        LittleFS.end();
        fsUnmounted_ = true;
    }
    return true;
}

bool OtaUpdateService::isProtectedRegion(uint32_t offset, const String &path) const
{
    String lower = path;
    lower.toLowerCase();
    return lower.indexOf(F("bootloader")) >= 0 || lower.indexOf(F("partition")) >= 0;
}
