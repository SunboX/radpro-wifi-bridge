// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>

namespace StallRecorder
{
enum class Stage : uint8_t
{
    Start, Update, TimeSync, UsbStart, Poll, UsbCommands, Settings, Wifi, Http,
    Mqtt, OpenSenseMap, GmcMap, Radmon, OpenRadiation, Safecast, Recovery, Idle,
    // Append only. Stage values are stored in flash in existing incident records.
    MqttConnect, MqttStatus, MqttLoop, MqttWrite, MqttClose
};
enum class Outcome : uint8_t { Observing, Recovered, UsbWatchdog, Suspended };

// Fixed-size records, with no pointers, credentials, or detector payloads.
struct Sample
{
    uint32_t uptimeMs, stageMs, pollMs, replyMs;
    uint32_t txStartMs, txEndMs, rxMs, txAttempts, txCompleted, txErrors, rxPackets;
    int32_t lastTxError;
    uint32_t generation, freeHeap;
    Stage stage;
    uint8_t command;
    bool usbConnected, wifiConnected, dnsPending, havePoll, haveReply;
    uint8_t wifiDisconnectReason;
    uint32_t wifiDisconnects, lastWifiDisconnectAgeMs;
    uint32_t mqttServiceAgeMs, maxMqttServiceGapMs;
    int8_t mqttState;
    bool haveMqttService;
};

struct Incident
{
    uint32_t bootId, startedUtc;
    Sample first, last;
    Outcome outcome;
};

struct History
{
    static constexpr uint8_t capacity = 3;
    uint8_t version = 1, next = 0, count = 0;
    Incident records[capacity]{};

    bool valid() const { return version == 1 && next < capacity && count <= capacity; }
    uint8_t newest() const { return (next + capacity - 1) % capacity; }

    void start(uint32_t bootId, uint32_t utc, const Sample &sample)
    {
        records[next] = {bootId, utc, sample, sample, Outcome::Observing};
        next = (next + 1) % capacity;
        if (count < capacity) ++count;
    }
};

// The sampler and the reboot path share this state machine. A restored record
// belongs to its original boot and is never implicitly closed on the next one.
class Monitor
{
public:
    History history;
    static constexpr uint32_t thresholdMs = 30000;

    bool observe(uint32_t bootId, uint32_t utc, const Sample &sample, bool armed,
                 uint32_t intervalMs, bool reboot = false)
    {
        if (restarting_) return false;
        const uint64_t silenceLimit = uint64_t(intervalMs) * 3;
        const bool silent = sample.replyMs >= thresholdMs && sample.replyMs >= silenceLimit;
        const bool blocked = sample.stageMs >= thresholdMs && sample.pollMs >= thresholdMs &&
                             sample.pollMs >= intervalMs;
        if (!active_)
        {
            if (!reboot && (!armed || (!silent && !blocked))) return false;
            history.start(bootId, utc, sample);
            if (!reboot)
            {
                active_ = true;
                return true;
            }
        }
        Incident &incident = history.records[history.newest()];
        if (armed && !reboot)
        {
            // Scheduling another poll isn't proof that the detector replied.
            const bool sameReply = sample.uptimeMs - sample.replyMs == incident.first.uptimeMs - incident.first.replyMs;
            if (silent || blocked || !sample.haveReply || sameReply) return false;
        }
        incident.last = sample;
        incident.outcome = reboot ? Outcome::UsbWatchdog : armed ? Outcome::Recovered : Outcome::Suspended;
        active_ = false;
        restarting_ = reboot;
        return true;
    }
private:
    bool active_ = false, restarting_ = false;
};
}
