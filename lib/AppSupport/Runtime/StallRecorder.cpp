// SPDX-License-Identifier: GPL-3.0-or-later
#include "Runtime/StallRecorder.h"
#include "DeviceManager.h"
#include "Network/BridgeDnsResolver.h"
#include <Preferences.h>
#include <esp_system.h>
#include <cerrno>
#include <ctime>
#include <mutex>

namespace StallRecorder
{
namespace
{
UsbCdcHost *usb = nullptr;
DeviceManager *detector = nullptr;
uint32_t bootId = 0;
struct Progress
{
    Stage stage = Stage::Start;
    uint32_t entered = 0, polled = 0, replied = 0, interval = 0;
    uint32_t disconnects = 0, disconnectedAt = 0;
    uint32_t mqttServiced = 0, maxMqttGap = 0;
    int8_t mqttState = 0;
    bool haveMqttService = false;
    uint8_t disconnectReason = 0;
    bool polling = false, havePoll = false, haveReply = false, wifi = false;
};
Progress live;
constexpr size_t mqttCallCount = static_cast<size_t>(Stage::MqttClose) - static_cast<size_t>(Stage::MqttConnect) + 1;
struct MqttTransportStats
{
    uint32_t maxCallMs[mqttCallCount]{};
    uint32_t failures = 0, timeouts = 0, lastFailureMs = 0, lastWriteMs = 0;
    int lastError = 0;
};
MqttTransportStats mqttTransport;
portMUX_TYPE progressLock = portMUX_INITIALIZER_UNLOCKED;
std::mutex historyLock;
Monitor monitor;
bool samplerRunning = false, pendingSave = false;
uint32_t writeErrors = 0, lastWriteMs = 0;
constexpr char storageName[] = "bridge-stalls";

const char *stageName(Stage stage)
{
    static const char *const names[] = {"startup", "updater", "time sync", "USB startup", "detector poll",
        "USB commands", "settings", "Wi-Fi", "HTTP portal", "MQTT", "OpenSenseMap", "GMCMap",
        "Radmon", "OpenRadiation", "Safecast", "recovery check", "loop delay",
        "MQTT connect", "MQTT socket status", "MQTT keepalive/read", "MQTT socket write", "MQTT socket close"};
    const auto index = static_cast<unsigned>(stage);
    return index < sizeof(names) / sizeof(names[0]) ? names[index] : "unknown";
}

Sample sample(bool &armed, uint32_t &interval)
{
    portENTER_CRITICAL(&progressLock);
    const Progress current = live;
    portEXIT_CRITICAL(&progressLock);
    const auto io = usb->transferProgress();
    const uint32_t now = millis();
    armed = current.polling && current.haveReply;
    interval = current.interval;
    return {now, now - current.entered, now - current.polled, now - current.replied,
            now - io.lastTxStartMs, now - io.lastTxEndMs, now - io.lastRxMs,
            io.txAttempts, io.txCompleted, io.txErrors, io.rxPackets, io.lastTxError,
            usb->connectionGeneration(), ESP.getFreeHeap(), current.stage,
            static_cast<uint8_t>(detector->lastQueuedType()), usb->isConnected(), current.wifi,
            BridgeDnsResolver::snapshot().pending, current.havePoll, current.haveReply, current.disconnectReason,
            current.disconnects, now - current.disconnectedAt, now - current.mqttServiced,
            current.maxMqttGap, current.mqttState, current.haveMqttService};
}

void observe(bool reboot)
{
    if (!usb) return;
    std::lock_guard<std::mutex> lock(historyLock);
    bool armed;
    uint32_t interval;
    const auto s = sample(armed, interval);
    const time_t epoch = time(nullptr);
    const bool changed = monitor.observe(bootId, epoch >= 1704067200 ? uint32_t(epoch) : 0,
                                         s, armed, interval, reboot);
    if (!changed && (!pendingSave || s.uptimeMs - lastWriteMs < 60000)) return;
    lastWriteMs = s.uptimeMs;
    Preferences prefs;
    pendingSave = !prefs.begin(storageName, false) ||
                  prefs.putBytes("history", &monitor.history, sizeof(History)) != sizeof(History);
    if (pendingSave) ++writeErrors;
}

void sampler(void *)
{
    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
        observe(false);
    }
}

void writeSample(JsonObject out, const Sample &s)
{
    out["uptimeMs"] = s.uptimeMs;
    out["stage"] = stageName(s.stage);
    out["stageAgeMs"] = s.stageMs;
    if (s.havePoll) out["lastPollAgeMs"] = s.pollMs;
    if (s.haveReply) out["lastTelemetryAgeMs"] = s.replyMs;
    out["lastQueuedCommand"] = DeviceManager::commandTypeName(static_cast<DeviceManager::CommandType>(s.command));
    out["usbConnected"] = s.usbConnected;
    out["connectionGeneration"] = s.generation;
    out["txAttempts"] = s.txAttempts;
    out["txCompleted"] = s.txCompleted;
    out["txErrors"] = s.txErrors;
    out["lastTxError"] = s.lastTxError;
    if (s.txAttempts) out["lastTxStartAgeMs"] = s.txStartMs;
    if (s.txCompleted) out["lastTxEndAgeMs"] = s.txEndMs;
    out["rxPackets"] = s.rxPackets;
    if (s.rxPackets) out["lastRxAgeMs"] = s.rxMs;
    out["wifiConnectedAtLastLoop"] = s.wifiConnected;
    out["dnsPending"] = s.dnsPending;
    if (s.haveMqttService)
    {
        out["lastMqttServiceAgeMs"] = s.mqttServiceAgeMs;
        out["maxMqttServiceGapMs"] = s.maxMqttServiceGapMs;
        out["lastMqttClientState"] = s.mqttState;
    }
    out["wifiDisconnects"] = s.wifiDisconnects;
    if (s.wifiDisconnects)
    {
        out["lastWifiDisconnectReason"] = s.wifiDisconnectReason;
        out["lastWifiDisconnectAgeMs"] = s.lastWifiDisconnectAgeMs;
    }
    out["heapFree"] = s.freeHeap;
}
}

bool begin(UsbCdcHost &host, DeviceManager &device, uint32_t currentBoot)
{
    usb = &host;
    detector = &device;
    bootId = currentBoot;
    Preferences prefs;
    History saved;
    if (prefs.begin(storageName, true) && prefs.getBytesLength("history") == sizeof(saved) &&
        prefs.getBytes("history", &saved, sizeof(saved)) == sizeof(saved) && saved.valid())
        monitor.history = saved;
    samplerRunning = xTaskCreate(sampler, "stall-capture", 4096, nullptr, 1, nullptr) == pdPASS;
    return samplerRunning;
}

Location enter(Stage stage)
{
    const uint32_t now = millis();
    portENTER_CRITICAL(&progressLock);
    const Location previous{live.stage, live.entered};
    live.stage = stage;
    live.entered = now;
    portEXIT_CRITICAL(&progressLock);
    return previous;
}

void restore(Location location)
{
    const uint32_t now = millis();
    portENTER_CRITICAL(&progressLock);
    if (live.stage >= Stage::MqttConnect && live.stage <= Stage::MqttClose)
    {
        auto &maximum = mqttTransport.maxCallMs[static_cast<size_t>(live.stage) - static_cast<size_t>(Stage::MqttConnect)];
        const uint32_t elapsed = now - live.entered;
        if (elapsed > maximum) maximum = elapsed;
    }
    live.stage = location.stage;
    live.entered = location.since;
    portEXIT_CRITICAL(&progressLock);
}

void setPolling(bool enabled, uint32_t intervalMs, bool wifiConnected)
{
    portENTER_CRITICAL(&progressLock);
    live.polling = enabled;
    if (!enabled) live.haveReply = false;
    live.interval = intervalMs;
    live.wifi = wifiConnected;
    portEXIT_CRITICAL(&progressLock);
}

void requestedPoll()
{
    const uint32_t now = millis();
    portENTER_CRITICAL(&progressLock);
    live.polled = now;
    live.havePoll = true;
    portEXIT_CRITICAL(&progressLock);
}

void receivedTelemetry()
{
    const uint32_t now = millis();
    portENTER_CRITICAL(&progressLock);
    live.replied = now;
    live.haveReply = live.polling;
    portEXIT_CRITICAL(&progressLock);
}

void wifiDisconnected(uint8_t reason)
{
    const uint32_t now = millis();
    portENTER_CRITICAL(&progressLock);
    ++live.disconnects;
    live.disconnectedAt = now;
    live.disconnectReason = reason;
    portEXIT_CRITICAL(&progressLock);
}

void mqttServiceFinished(int clientState)
{
    const uint32_t now = millis();
    portENTER_CRITICAL(&progressLock);
    const uint32_t gap = now - live.mqttServiced;
    if (live.haveMqttService && gap > live.maxMqttGap) live.maxMqttGap = gap;
    live.mqttServiced = now;
    live.haveMqttService = true;
    live.mqttState = static_cast<int8_t>(clientState);
    portEXIT_CRITICAL(&progressLock);
}

void beforeWatchdogRestart() { observe(true); }

void mqttWriteFailed(uint32_t elapsedMs, int error)
{
    const uint32_t now = millis();
    portENTER_CRITICAL(&progressLock);
    ++mqttTransport.failures;
    if (error == ETIMEDOUT) ++mqttTransport.timeouts;
    mqttTransport.lastError = error;
    mqttTransport.lastFailureMs = now;
    mqttTransport.lastWriteMs = elapsedMs;
    portEXIT_CRITICAL(&progressLock);
}

void appendJson(JsonObject out)
{
    out["samplerRunning"] = samplerRunning;
    out["bootId"] = bootId;
    out["thresholdMs"] = Monitor::thresholdMs;
    portENTER_CRITICAL(&progressLock);
    const MqttTransportStats stats = mqttTransport;
    portEXIT_CRITICAL(&progressLock);
    JsonObject transport = out["mqttTransport"].to<JsonObject>();
    JsonObject durations = transport["maxCallMs"].to<JsonObject>();
    for (size_t i = 0; i < mqttCallCount; ++i)
        durations[stageName(static_cast<Stage>(static_cast<size_t>(Stage::MqttConnect) + i))] = stats.maxCallMs[i];
    transport["writeFailures"] = stats.failures;
    transport["writeTimeouts"] = stats.timeouts;
    if (stats.failures)
    {
        transport["lastWriteErrno"] = stats.lastError;
        transport["lastFailedWriteMs"] = stats.lastWriteMs;
        transport["lastFailureUptimeMs"] = stats.lastFailureMs;
    }
    if (usb)
    {
        bool armed;
        uint32_t interval;
        const auto current = sample(armed, interval);
        out["armed"] = armed;
        out["pollIntervalMs"] = interval;
        writeSample(out["current"].to<JsonObject>(), current);
        const auto rx = usb->receiveStatus();
        JsonObject receive = out["usbReceive"].to<JsonObject>();
        receive["callbacksReturned"] = rx.callbacksReturned;
        receive["failures"] = rx.failures;
        receive["hostRecoveries"] = rx.recoveries;
        if (rx.failures)
        {
            receive["lastFailureUptimeMs"] = rx.lastFailureMs;
            receive["lastFailure"] = rx.resubmitFailed ? "resubmit" : "transfer";
            receive["lastError"] = rx.error;
        }
    }
    History copy;
    {
        std::lock_guard<std::mutex> lock(historyLock);
        copy = monitor.history;
        out["storageErrors"] = writeErrors;
        out["pendingSave"] = pendingSave;
    }
    JsonArray records = out["incidents"].to<JsonArray>();
    for (uint8_t i = 0; i < copy.count; ++i)
    {
        const Incident &r = copy.records[(copy.next + History::capacity - copy.count + i) % History::capacity];
        JsonObject item = records.add<JsonObject>();
        item["bootId"] = r.bootId;
        if (r.startedUtc) item["deviceEpoch"] = r.startedUtc;
        const char *outcome = "interrupted";
        switch (r.outcome)
        {
        case Outcome::Observing: if (r.bootId == bootId) outcome = "observing"; break;
        case Outcome::Recovered: outcome = "recovered"; break;
        case Outcome::UsbWatchdog: outcome = "USB watchdog restart"; break;
        case Outcome::Suspended: outcome = "polling suspended"; break;
        }
        item["outcome"] = outcome;
        writeSample(item["first"].to<JsonObject>(), r.first);
        if (r.outcome != Outcome::Observing)
        {
            item["closedUptimeMs"] = r.last.uptimeMs;
            if (r.outcome == Outcome::UsbWatchdog) writeSample(item["atRestart"].to<JsonObject>(), r.last);
            if (r.outcome == Outcome::Recovered) writeSample(item["atRecovery"].to<JsonObject>(), r.last);
        }
    }
}
}
