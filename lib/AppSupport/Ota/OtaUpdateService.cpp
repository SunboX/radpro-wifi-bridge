#include "Ota/OtaUpdateService.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <esp_flash.h>

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
        parts_.push_back(info);
    }

    targetVersion_ = doc["version"].as<const char *>();
    busy_ = true;
    needsReboot_ = false;
    fsUnmounted_ = false;
    active_ = ActivePart{};
    lastError_ = String();
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
        if (!part.received && part.path == path && part.offset == offset)
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

    if (!ensureFsUnmounted(offset, path))
        return false;

    if (!eraseRegion(offset, size))
        return false;

    active_.info = target;
    active_.offset = offset;
    active_.expectedSize = size;
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

    if (active_.written + len > active_.expectedSize)
    {
        lastError_ = F("Chunk exceeds expected size.");
        return false;
    }

    esp_err_t err = esp_flash_write(nullptr, data, active_.offset + active_.written, len);
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

    if (active_.written != active_.expectedSize)
    {
        lastError_ = F("Part size mismatch.");
        return false;
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
        if (!part.received)
        {
            lastError_ = F("Missing part data.");
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
