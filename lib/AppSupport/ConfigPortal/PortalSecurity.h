/*
 * SPDX-FileCopyrightText: 2026 André Fiedler
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <Arduino.h>
#include <vector>

namespace PortalSecurity
{
    constexpr const char *kCsrfFieldName = "csrf";

    inline bool isValidCsrfToken(const String &provided, const String &expected)
    {
        if (!provided.length() || !expected.length())
            return false;
        if (provided.length() != expected.length())
            return false;

        uint8_t diff = 0;
        for (size_t i = 0; i < provided.length(); ++i)
            diff |= static_cast<uint8_t>(provided[i] ^ expected[i]);
        return diff == 0;
    }

    inline std::vector<String> missingRequiredFields(const std::vector<const char *> &required,
                                                     const std::vector<String> &present)
    {
        std::vector<String> missing;
        for (const char *field : required)
        {
            bool found = false;
            for (const String &name : present)
            {
                if (name == field)
                {
                    found = true;
                    break;
                }
            }
            if (!found)
                missing.emplace_back(field);
        }
        return missing;
    }

    inline void appendChangedField(std::vector<String> &changedFields, const char *fieldName, bool changed)
    {
        if (changed)
            changedFields.emplace_back(fieldName);
    }

    inline String joinFieldNames(const std::vector<String> &fields)
    {
        String out;
        for (size_t i = 0; i < fields.size(); ++i)
        {
            if (i > 0)
                out += ",";
            out += fields[i];
        }
        return out;
    }

    inline void logConfigSave(Print &log, const char *route, const String &clientIp, const std::vector<String> &changedFields)
    {
        log.print("Config save via ");
        log.print(route ? route : "<unknown>");
        log.print(" from ");
        log.print(clientIp.length() ? clientIp : String("<unknown>"));
        log.print("; changed fields=");
        log.println(changedFields.empty() ? String("<none>") : joinFieldNames(changedFields));
    }

    inline void logConfigSaveFailure(Print &log, const char *route, const String &clientIp, const std::vector<String> &changedFields)
    {
        log.print("Config save failed via ");
        log.print(route ? route : "<unknown>");
        log.print(" from ");
        log.print(clientIp.length() ? clientIp : String("<unknown>"));
        log.print("; changed fields=");
        log.println(changedFields.empty() ? String("<none>") : joinFieldNames(changedFields));
    }
}
