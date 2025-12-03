#include "Time/TimeSync.h"

#include <esp_sntp.h>

#include "Logging/DebugLogStream.h"

namespace
{
    constexpr time_t kMinValidEpoch = 1704067200; // 2024-01-01
    constexpr unsigned long kRetryMs = 10000;
}

TimeSync::TimeSync(DebugLogStream &log)
    : log_(log)
{
}

void TimeSync::startSntp()
{
    if (sntpStarted_)
        return;

    setenv("TZ", "UTC", 1);
    tzset();
    sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
    sntp_set_sync_interval(6UL * 60UL * 60UL * 1000UL);
    configTime(0, 0, "pool.ntp.org", "time.nist.gov", "time.google.com");
    sntpStarted_ = true;
    lastKickMs_ = millis();
    log_.println(F("NTP sync requested (pool.ntp.org/time.nist.gov/time.google.com)."));
}

bool TimeSync::hasValidRtc() const
{
    return time(nullptr) >= kMinValidEpoch;
}

void TimeSync::markSynced(time_t now)
{
    synced_ = true;
    waitLogged_ = false;
    lastSyncEpoch_ = now;

    struct tm tmInfo;
    gmtime_r(&now, &tmInfo);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S UTC", &tmInfo);
    log_.print("Time synchronized: ");
    log_.println(buf);
}

void TimeSync::requestResync()
{
    synced_ = false;
    waitLogged_ = false;
    sntpStarted_ = false;
    lastKickMs_ = 0;
}

void TimeSync::loop(bool wifiConnected)
{
    if (synced_ && hasValidRtc())
        return;

    time_t now = time(nullptr);
    if (now >= kMinValidEpoch)
    {
        markSynced(now);
        return;
    }

    if (!wifiConnected)
        return;

    if (!sntpStarted_)
    {
        startSntp();
    }
    else if (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET &&
             millis() - lastKickMs_ >= kRetryMs)
    {
        log_.println(F("NTP sync retry…"));
        configTime(0, 0, "pool.ntp.org", "time.nist.gov", "time.google.com");
        lastKickMs_ = millis();
    }

    if (!waitLogged_)
    {
        log_.println(F("Waiting for NTP time sync…"));
        waitLogged_ = true;
    }
}
