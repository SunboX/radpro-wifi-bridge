// SPDX-FileCopyrightText: 2026 André Fiedler
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Logging/DebugLogStream.h"

#include <cstdio>
#include <ctime>
#include <sys/time.h>
#include <esp_timer.h>

DebugLogStream::DebugLogStream(HardwareSerial &serial, size_t maxEntries)
    : serial_(serial),
      maxEntries_(maxEntries ? maxEntries : 1),
      nextId_(1),
      mutex_(xSemaphoreCreateMutex())
{
}

DebugLogStream::~DebugLogStream()
{
    if (mutex_)
        vSemaphoreDelete(mutex_);
}

void DebugLogStream::begin(unsigned long baud)
{
    serial_.begin(baud);
}

void DebugLogStream::begin(unsigned long baud, uint32_t config)
{
    serial_.begin(baud, config);
}

void DebugLogStream::end()
{
    serial_.end();
}

int DebugLogStream::available()
{
    return serial_.available();
}

int DebugLogStream::read()
{
    return serial_.read();
}

int DebugLogStream::peek()
{
    return serial_.peek();
}

void DebugLogStream::flush()
{
    serial_.flush();
}

size_t DebugLogStream::write(uint8_t ch)
{
    return write(&ch, 1);
}

size_t DebugLogStream::write(const uint8_t *buffer, size_t size)
{
    if (!size)
        return 0;
    if (!mutex_ || xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE)
        return serial_.write(buffer, size);

    size_t written = 0;
    while (written < size)
    {
        if (atLineStart_)
            startLineLocked();

        size_t end = written;
        while (end < size && buffer[end] != '\n')
            ++end;
        if (end < size)
            ++end;

        const size_t requested = end - written;
        const size_t count = serial_.write(buffer + written, requested);
        for (size_t i = 0; i < count; ++i)
            appendCharLocked(static_cast<char>(buffer[written + i]));
        written += count;
        if (count < requested)
            break;
    }
    xSemaphoreGive(mutex_);
    return written;
}

void DebugLogStream::startLineLocked()
{
    char prefix[48];
    timeval now{};
    constexpr time_t kMinValidEpoch = 1704067200; // 2024-01-01, as in TimeSync.
    if (gettimeofday(&now, nullptr) == 0 && now.tv_sec >= kMinValidEpoch)
    {
        tm utc{};
        gmtime_r(&now.tv_sec, &utc);
        snprintf(prefix, sizeof(prefix), "[%02d:%02d:%02d.%03ld UTC] ",
                 utc.tm_hour, utc.tm_min, utc.tm_sec, static_cast<long>(now.tv_usec / 1000));
    }
    else
    {
        const uint64_t uptimeMs = static_cast<uint64_t>(esp_timer_get_time()) / 1000;
        snprintf(prefix, sizeof(prefix), "[up %02llu:%02u:%02u.%03u] ",
                 static_cast<unsigned long long>(uptimeMs / 3600000),
                 static_cast<unsigned>((uptimeMs / 60000) % 60),
                 static_cast<unsigned>((uptimeMs / 1000) % 60),
                 static_cast<unsigned>(uptimeMs % 1000));
    }
    linePrefix_ = prefix;
    serial_.print(linePrefix_);
    atLineStart_ = false;
}

void DebugLogStream::copyEntries(std::vector<DebugLogEntry> &out) const
{
    out.clear();
    if (!mutex_)
        return;
    if (xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE)
        return;

    out.reserve(entries_.size() + (currentLine_.length() ? 1 : 0));
    for (const auto &entry : entries_)
    {
        out.push_back(entry);
    }

    if (currentLine_.length())
    {
        DebugLogEntry pending{nextId_, currentLine_};
        out.push_back(pending);
    }

    xSemaphoreGive(mutex_);
}

uint32_t DebugLogStream::latestId() const
{
    uint32_t latest = nextId_ ? nextId_ - 1 : 0;
    if (!mutex_)
        return latest;
    if (xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE)
        return latest;

    if (!entries_.empty())
        latest = entries_.back().id;
    else if (currentLine_.length())
        latest = nextId_;

    xSemaphoreGive(mutex_);
    return latest;
}

size_t DebugLogStream::entryCount() const
{
    size_t count = 0;
    if (!mutex_)
        return count;
    if (xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE)
        return count;

    count = entries_.size();
    if (currentLine_.length())
        ++count;

    xSemaphoreGive(mutex_);
    return count;
}

void DebugLogStream::appendCharLocked(char c)
{
    if (c == '\r')
        return;

    if (!currentLine_.length())
        currentLine_ = linePrefix_;

    if (c == '\n')
    {
        pushLineLocked(currentLine_);
        currentLine_.clear();
        atLineStart_ = true;
        return;
    }

    currentLine_ += c;

    constexpr size_t kMaxLineLength = 320;
    if (currentLine_.length() >= kMaxLineLength)
    {
        pushLineLocked(currentLine_);
        currentLine_.clear();
    }
}

void DebugLogStream::pushLineLocked(const String &line)
{
    DebugLogEntry entry;
    entry.id = nextId_++;
    entry.text = line;
    entries_.push_back(entry);
    if (entries_.size() > maxEntries_)
    {
        entries_.erase(entries_.begin());
    }
}
