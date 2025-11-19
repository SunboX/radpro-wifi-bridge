#pragma once

#include <Arduino.h>
#include <vector>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

struct DebugLogEntry
{
    uint32_t id;
    String text;
};

class DebugLogStream : public Stream
{
public:
    DebugLogStream(HardwareSerial &serial, size_t maxEntries = 400);
    ~DebugLogStream();

    void begin(unsigned long baud);
    void begin(unsigned long baud, uint32_t config);
    void end();

    int available() override;
    int read() override;
    int peek() override;
    void flush() override;

    size_t write(uint8_t ch) override;
    size_t write(const uint8_t *buffer, size_t size) override;
    using Print::write;

    void copyEntries(std::vector<DebugLogEntry> &out) const;
    uint32_t latestId() const;
    size_t entryCount() const;
    size_t maxEntries() const { return maxEntries_; }

private:
    void appendCharLocked(char c);
    void pushLineLocked(const String &line);

    HardwareSerial &serial_;
    size_t maxEntries_;
    String currentLine_;
    std::vector<DebugLogEntry> entries_;
    uint32_t nextId_;
    SemaphoreHandle_t mutex_;
};
