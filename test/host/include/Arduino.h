/*
 * SPDX-FileCopyrightText: 2026 André Fiedler
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>

class String
{
public:
    String() = default;
    String(const char *value) : value_(value ? value : "") {}
    String(const std::string &value) : value_(value) {}
    String(char value) : value_(1, value) {}
    String(int value) : value_(std::to_string(value)) {}
    String(unsigned int value) : value_(std::to_string(value)) {}
    String(long value) : value_(std::to_string(value)) {}
    String(unsigned long value) : value_(std::to_string(value)) {}
    String(float value)
    {
        std::ostringstream stream;
        stream << value;
        value_ = stream.str();
    }
    String(float value, unsigned int decimals)
    {
        std::ostringstream stream;
        stream << std::fixed << std::setprecision(static_cast<int>(decimals)) << value;
        value_ = stream.str();
    }
    String(double value)
    {
        std::ostringstream stream;
        stream << value;
        value_ = stream.str();
    }
    String(double value, unsigned int decimals)
    {
        std::ostringstream stream;
        stream << std::fixed << std::setprecision(static_cast<int>(decimals)) << value;
        value_ = stream.str();
    }

    String &operator+=(const char *value)
    {
        value_ += value ? value : "";
        return *this;
    }

    String &operator+=(const String &value)
    {
        value_ += value.value_;
        return *this;
    }

    String &operator+=(char value)
    {
        value_ += value;
        return *this;
    }

    bool startsWith(const char *prefix) const
    {
        if (!prefix)
            return false;
        size_t len = std::strlen(prefix);
        return value_.size() >= len && value_.compare(0, len, prefix) == 0;
    }

    bool equalsIgnoreCase(const char *other) const
    {
        if (!other)
            return false;
        const size_t otherLen = std::strlen(other);
        if (value_.size() != otherLen)
            return false;
        for (size_t i = 0; i < otherLen; ++i)
        {
            if (std::tolower(static_cast<unsigned char>(value_[i])) !=
                std::tolower(static_cast<unsigned char>(other[i])))
                return false;
        }
        return true;
    }

    bool equalsIgnoreCase(const String &other) const
    {
        return equalsIgnoreCase(other.c_str());
    }

    void trim()
    {
        auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
        auto begin = std::find_if(value_.begin(), value_.end(), notSpace);
        auto end = std::find_if(value_.rbegin(), value_.rend(), notSpace).base();
        if (begin >= end)
        {
            value_.clear();
            return;
        }
        value_ = std::string(begin, end);
    }

    size_t length() const
    {
        return value_.size();
    }

    char operator[](size_t index) const
    {
        return value_[index];
    }

    int indexOf(char needle, size_t from = 0) const
    {
        size_t pos = value_.find(needle, from);
        return pos == std::string::npos ? -1 : static_cast<int>(pos);
    }

    int indexOf(const char *needle, size_t from = 0) const
    {
        if (!needle)
            return -1;
        size_t pos = value_.find(needle, from);
        return pos == std::string::npos ? -1 : static_cast<int>(pos);
    }

    int indexOf(const String &needle, size_t from = 0) const
    {
        return indexOf(needle.c_str(), from);
    }

    int lastIndexOf(char needle) const
    {
        size_t pos = value_.rfind(needle);
        return pos == std::string::npos ? -1 : static_cast<int>(pos);
    }

    String substring(size_t start) const
    {
        if (start >= value_.size())
            return String();
        return String(value_.substr(start));
    }

    String substring(size_t start, size_t end) const
    {
        if (start >= value_.size() || end <= start)
            return String();
        return String(value_.substr(start, end - start));
    }

    long toInt() const
    {
        try
        {
            size_t idx = 0;
            long value = std::stol(value_, &idx);
            return idx == 0 ? 0L : value;
        }
        catch (...)
        {
            return 0L;
        }
    }

    float toFloat() const
    {
        try
        {
            size_t idx = 0;
            float value = std::stof(value_, &idx);
            return idx == 0 ? 0.0f : value;
        }
        catch (...)
        {
            return 0.0f;
        }
    }

    const char *c_str() const
    {
        return value_.c_str();
    }

    friend bool operator==(const String &lhs, const String &rhs)
    {
        return lhs.value_ == rhs.value_;
    }

    friend bool operator!=(const String &lhs, const String &rhs)
    {
        return !(lhs == rhs);
    }

    operator std::string() const
    {
        return value_;
    }

    friend String operator+(const String &lhs, const char *rhs)
    {
        String result(lhs);
        result += rhs;
        return result;
    }

    friend String operator+(const char *lhs, const String &rhs)
    {
        String result(lhs);
        result += rhs;
        return result;
    }

    friend String operator+(const String &lhs, const String &rhs)
    {
        String result(lhs);
        result += rhs;
        return result;
    }

private:
    std::string value_;
};

class Print
{
public:
    virtual ~Print() = default;
    virtual size_t write(uint8_t ch) = 0;

    size_t write(const uint8_t *buffer, size_t size)
    {
        size_t written = 0;
        for (size_t i = 0; i < size; ++i)
            written += write(buffer[i]);
        return written;
    }

    size_t print(const char *text)
    {
        if (!text)
            return 0;
        return write(reinterpret_cast<const uint8_t *>(text), std::strlen(text));
    }

    size_t print(const String &text)
    {
        return print(text.c_str());
    }

    size_t print(char ch)
    {
        return write(static_cast<uint8_t>(ch));
    }

    size_t print(int value)
    {
        return print(std::to_string(value).c_str());
    }

    size_t print(unsigned int value)
    {
        return print(std::to_string(value).c_str());
    }

    size_t print(long value)
    {
        return print(std::to_string(value).c_str());
    }

    size_t print(unsigned long value)
    {
        return print(std::to_string(value).c_str());
    }

    size_t println()
    {
        return print("\n");
    }

    size_t println(const char *text)
    {
        size_t written = print(text);
        return written + println();
    }

    size_t println(const String &text)
    {
        size_t written = print(text);
        return written + println();
    }
};

inline unsigned long g_test_millis = 0;
inline uint8_t g_neopixel_pin = 0;
inline uint8_t g_neopixel_r = 0;
inline uint8_t g_neopixel_g = 0;
inline uint8_t g_neopixel_b = 0;

inline unsigned long millis()
{
    return g_test_millis;
}

inline void setMillis(unsigned long value)
{
    g_test_millis = value;
}

inline void advanceMillis(unsigned long delta)
{
    g_test_millis += delta;
}

inline void delay(unsigned long)
{
}

inline void yield()
{
}

inline void neopixelWrite(uint8_t pin, uint8_t r, uint8_t g, uint8_t b)
{
    g_neopixel_pin = pin;
    g_neopixel_r = r;
    g_neopixel_g = g;
    g_neopixel_b = b;
}
