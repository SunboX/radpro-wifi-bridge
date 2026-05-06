/*
 * SPDX-FileCopyrightText: 2026 André Fiedler
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include "Arduino.h"

class Preferences
{
public:
    bool begin(const char *name, bool readOnly = false)
    {
        namespace_ = name ? name : "";
        readOnly_ = readOnly;
        return true;
    }

    void end() {}

    String getString(const char *key, const String &defaultValue = String()) const
    {
        const auto it = store_.find(scopedKey(key));
        return it == store_.end() ? defaultValue : String(it->second);
    }

    bool getBool(const char *key, bool defaultValue = false) const
    {
        const auto it = store_.find(scopedKey(key));
        return it == store_.end() ? defaultValue : it->second == "1";
    }

    uint16_t getUShort(const char *key, uint16_t defaultValue = 0) const
    {
        const auto it = store_.find(scopedKey(key));
        return it == store_.end() ? defaultValue : static_cast<uint16_t>(std::stoul(it->second));
    }

    uint32_t getUInt(const char *key, uint32_t defaultValue = 0) const
    {
        const auto it = store_.find(scopedKey(key));
        return it == store_.end() ? defaultValue : static_cast<uint32_t>(std::stoul(it->second));
    }

    float getFloat(const char *key, float defaultValue = 0.0f) const
    {
        const auto it = store_.find(scopedKey(key));
        return it == store_.end() ? defaultValue : std::stof(it->second);
    }

    size_t putString(const char *key, const String &value)
    {
        if (readOnly_)
            return 0;
        store_[scopedKey(key)] = value.c_str();
        return value.length();
    }

    size_t putBool(const char *key, bool value)
    {
        if (readOnly_)
            return 0;
        store_[scopedKey(key)] = value ? "1" : "0";
        return 1;
    }

    size_t putUShort(const char *key, uint16_t value)
    {
        if (readOnly_)
            return 0;
        store_[scopedKey(key)] = std::to_string(value);
        return sizeof(value);
    }

    size_t putUInt(const char *key, uint32_t value)
    {
        if (readOnly_)
            return 0;
        store_[scopedKey(key)] = std::to_string(value);
        return sizeof(value);
    }

    size_t putFloat(const char *key, float value)
    {
        if (readOnly_)
            return 0;
        store_[scopedKey(key)] = std::to_string(value);
        return sizeof(value);
    }

private:
    std::string scopedKey(const char *key) const
    {
        return namespace_ + ":" + (key ? key : "");
    }

    inline static std::unordered_map<std::string, std::string> store_{};
    std::string namespace_;
    bool readOnly_ = false;
};
