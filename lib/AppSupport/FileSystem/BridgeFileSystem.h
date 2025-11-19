#pragma once

#include <Arduino.h>

namespace BridgeFileSystem
{
    constexpr const char *kBasePath = "/littlefs";
    constexpr const char *kLabel = "spiffs";
    constexpr uint8_t kMaxFiles = 10;

    bool mount(Print &log, const char *stage, bool formatOnFail);
    void logStats(Print &log, const char *stage);
    void dumpTree(Print &log, const char *reason);
    void dumpTree(Print &log, const __FlashStringHelper *reason);
}
