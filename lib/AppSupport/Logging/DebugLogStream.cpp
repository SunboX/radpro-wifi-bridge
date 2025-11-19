#include "Logging/DebugLogStream.h"

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
    size_t written = serial_.write(ch);
    if (mutex_ && xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE)
    {
        appendCharLocked(static_cast<char>(ch));
        xSemaphoreGive(mutex_);
    }
    return written;
}

size_t DebugLogStream::write(const uint8_t *buffer, size_t size)
{
    size_t written = serial_.write(buffer, size);
    if (size && mutex_ && xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE)
    {
        for (size_t i = 0; i < size; ++i)
        {
            appendCharLocked(static_cast<char>(buffer[i]));
        }
        xSemaphoreGive(mutex_);
    }
    return written;
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

    if (c == '\n')
    {
        pushLineLocked(currentLine_);
        currentLine_.clear();
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
