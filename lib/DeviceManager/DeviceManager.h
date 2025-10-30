#pragma once

#include <Arduino.h>
#include <functional>
#include <utility>
#include <vector>
#include "UsbCdcHost.h"

class DeviceManager
{
public:
    using LineHandler = std::function<void(const String &)>;
    using RawHandler = std::function<void(const uint8_t *, size_t)>;

    explicit DeviceManager(UsbCdcHost &host);

    void begin(uint16_t vid, uint16_t pid);
    void loop();

    enum class CommandType
    {
        DeviceId,
        DeviceModel,
        DeviceFirmware,
        DeviceLocale,
        DevicePower,
        DeviceBatteryVoltage,
        DeviceBatteryPercent,
        DeviceTime,
        DeviceTimeZone,
        DeviceSensitivity,
        TubeDoseRate,
        TubeTime,
        TubePulseCount,
        TubeRate,
        TubeDeadTime,
        TubeDeadTimeCompensation,
        TubeHVFrequency,
        TubeHVDutyCycle,
        RandomData,
        DataLog,
        Generic
    };

    using CommandResultHandler = std::function<void(CommandType, const String &, bool)>;

    void setLineHandler(LineHandler handler) { line_handler_ = std::move(handler); }
    void setRawHandler(RawHandler handler) { raw_handler_ = std::move(handler); }
    void setCommandResultHandler(CommandResultHandler handler) { command_result_handler_ = std::move(handler); }

    void setRawLogging(bool enabled);
    void toggleRawLogging();
    bool rawLoggingEnabled() const { return raw_logging_enabled_; }
    void setVerboseLogging(bool enabled) { verbose_logging_enabled_ = enabled; }
    bool verboseLoggingEnabled() const { return verbose_logging_enabled_; }

    void start();
    void stop();
    void enable(bool active);
    bool enabled() const { return enabled_; }
    void requestStats();
    void requestRandomData();
    void requestDataLog(const String &args = String());
    bool hasSensitivity() const { return device_sensitivity_cpm_per_uSv_ > 0.0f; }

private:

    struct PendingCommand
    {
        String command;
        CommandType type;
        bool announce;
        uint8_t retry;
        unsigned long ready_ms;
    };

    static DeviceManager *instance_;

    static void HandleConnected();
    static void HandleDisconnected();
    static void HandleLine(const String &line);
    static void HandleRaw(const uint8_t *data, size_t len);

    void onConnected();
    void onDisconnected();
    void onLine(const String &line);
    void onRaw(const uint8_t *data, size_t len);

    void scheduleDeviceId(uint32_t delay_ms, bool announce);
    void enqueueCommand(const String &cmd, CommandType type, uint32_t delay_ms, bool announce);
    bool isCommandPending(const String &cmd) const;
    void processQueue();
    void issueCurrentCommand();
    void handleSuccess();
    void handleError();
    void emitResult(CommandType type, const String &value, bool success);

    UsbCdcHost &host_;

    LineHandler line_handler_ = nullptr;
    RawHandler raw_handler_ = nullptr;
    CommandResultHandler command_result_handler_ = nullptr;

    bool raw_logging_enabled_ = false;
    bool verbose_logging_enabled_ = false;
    bool enabled_ = false;
    bool device_id_logged_ = false;
    bool device_details_logged_ = false;
    bool awaiting_response_ = false;
    bool has_current_command_ = false;
    PendingCommand current_command_{};
    std::vector<PendingCommand> command_queue_;
    unsigned long last_request_ms_ = 0;
    float device_sensitivity_cpm_per_uSv_ = 0.0f;
};
