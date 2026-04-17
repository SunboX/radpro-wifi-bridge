#pragma once

#include <Arduino.h>

struct PublisherHealthSnapshot
{
    bool enabled = false;
    bool paused = false;
    bool pending = false;
    uint32_t attempts = 0;
    uint32_t successes = 0;
    uint32_t failures = 0;
    uint32_t consecutiveFailures = 0;
    unsigned long lastAttemptMs = 0;
    unsigned long lastSuccessMs = 0;
    unsigned long lastFailureMs = 0;
    int lastStatusCode = 0;
    String lastStatusLine;
    String lastError;
    String lastResponseTrace;
};

class PublisherHealth
{
public:
    void setEnabled(bool enabled) { snapshot_.enabled = enabled; }
    void setPaused(bool paused) { snapshot_.paused = paused; }
    void setPending(bool pending) { snapshot_.pending = pending; }

    void noteAttempt(unsigned long now)
    {
        snapshot_.attempts += 1;
        snapshot_.lastAttemptMs = now;
    }

    void noteSuccess(unsigned long now,
                     int statusCode,
                     const String &statusLine = String())
    {
        snapshot_.successes += 1;
        snapshot_.consecutiveFailures = 0;
        snapshot_.lastSuccessMs = now;
        snapshot_.lastStatusCode = statusCode;
        snapshot_.lastStatusLine = statusLine;
        snapshot_.lastError = String();
        snapshot_.lastResponseTrace = String();
    }

    void noteFailure(unsigned long now,
                     const String &error,
                     int statusCode = 0,
                     const String &statusLine = String(),
                     const String &responseTrace = String())
    {
        snapshot_.failures += 1;
        snapshot_.consecutiveFailures += 1;
        snapshot_.lastFailureMs = now;
        snapshot_.lastStatusCode = statusCode;
        snapshot_.lastStatusLine = statusLine;
        snapshot_.lastError = error;
        snapshot_.lastResponseTrace = responseTrace;
    }

    const PublisherHealthSnapshot &snapshot() const { return snapshot_; }

private:
    PublisherHealthSnapshot snapshot_;
};
