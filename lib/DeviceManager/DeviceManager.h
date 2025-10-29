#pragma once

#include <Arduino.h>
#include <functional>
#include <utility>
#include "UsbCdcHost.h"

class DeviceManager
{
public:
    using LineHandler = std::function<void(const String &)>;
    using RawHandler = std::function<void(const uint8_t *, size_t)>;

    explicit DeviceManager(UsbCdcHost &host);

    void begin(uint16_t vid, uint16_t pid);
    void loop();

    void setLineHandler(LineHandler handler) { line_handler_ = std::move(handler); }
    void setRawHandler(RawHandler handler) { raw_handler_ = std::move(handler); }

    void setRawLogging(bool enabled);
    void toggleRawLogging();
    bool rawLoggingEnabled() const { return raw_logging_enabled_; }

    void start();
    void stop();
    void enable(bool active);
    bool enabled() const { return enabled_; }

private:
    static DeviceManager *instance_;

    static void HandleConnected();
    static void HandleDisconnected();
    static void HandleLine(const String &line);
    static void HandleRaw(const uint8_t *data, size_t len);

    void onConnected();
    void onDisconnected();
    void onLine(const String &line);
    void onRaw(const uint8_t *data, size_t len);

    void scheduleRequest(uint32_t delay_ms, bool announce);
    void issueRequest(bool announce);

    UsbCdcHost &host_;

    LineHandler line_handler_ = nullptr;
    RawHandler raw_handler_ = nullptr;

    bool raw_logging_enabled_ = false;
    bool enabled_ = false;
        bool device_id_logged_ = false;
        bool device_details_logged_ = false;
    bool request_issued_ = false;
    bool awaiting_response_ = false;
    bool announce_next_ = true;
    uint8_t retry_count_ = 0;
    unsigned long request_time_ms_ = 0;
    unsigned long last_request_ms_ = 0;
};
