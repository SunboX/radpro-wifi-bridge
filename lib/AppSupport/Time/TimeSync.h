#pragma once

#include <Arduino.h>
#include <time.h>

class DebugLogStream;

class TimeSync
{
public:
    explicit TimeSync(DebugLogStream &log);

    void loop(bool wifiConnected);
    bool synced() const { return synced_; }
    time_t lastSyncEpoch() const { return lastSyncEpoch_; }
    void requestResync();

private:
    void startSntp();
    bool hasValidRtc() const;
    void markSynced(time_t now);

    DebugLogStream &log_;
    bool sntpStarted_ = false;
    bool synced_ = false;
    bool waitLogged_ = false;
    unsigned long lastKickMs_ = 0;
    time_t lastSyncEpoch_ = 0;
};
