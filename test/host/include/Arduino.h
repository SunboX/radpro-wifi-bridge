#pragma once

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

class String
{
public:
    String() = default;
    String(const char *value) : value_(value ? value : "") {}
    String(const std::string &value) : value_(value) {}

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

    bool startsWith(const char *prefix) const
    {
        if (!prefix)
            return false;
        size_t len = std::strlen(prefix);
        return value_.size() >= len && value_.compare(0, len, prefix) == 0;
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

    const char *c_str() const
    {
        return value_.c_str();
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
