#pragma once

#include <Arduino.h>

namespace RadmonPortalLinks
{
inline String trimUser(const String &value)
{
    String trimmed = value;
    trimmed.trim();
    return trimmed;
}

inline String urlEncode(const String &input)
{
    static const char *hex = "0123456789ABCDEF";
    String out;
    for (size_t i = 0; i < input.length(); ++i)
    {
        const unsigned char c = static_cast<unsigned char>(input[i]);
        if ((c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~')
        {
            out += static_cast<char>(c);
        }
        else
        {
            out += '%';
            out += hex[(c >> 4) & 0x0F];
            out += hex[c & 0x0F];
        }
    }
    return out;
}

inline String buildRadmonStationUrl(const String &user)
{
    const String trimmedUser = trimUser(user);
    if (!trimmedUser.length())
        return String();

    return String("https://radmon.org/radmon.php?function=showuserpage&user=") + urlEncode(trimmedUser);
}
} // namespace RadmonPortalLinks
