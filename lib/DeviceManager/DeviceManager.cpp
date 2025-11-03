#include <DeviceManager.h>
#include <time.h>

namespace
{
    constexpr uint32_t DEVICE_ID_INITIAL_DELAY_MS = 2500;
    constexpr uint32_t DEVICE_ID_RETRY_DELAY_MS = 1500;
    constexpr uint32_t DEVICE_ID_RESPONSE_TIMEOUT_MS = 3000;
    constexpr uint8_t DEVICE_ID_MAX_RETRY = 4;
    constexpr const char *DEVICE_KEEPALIVE_LINE = "Main loop is running.";
}

DeviceManager *DeviceManager::instance_ = nullptr;

void DeviceManager::HandleConnected()
{
    if (instance_)
        instance_->onConnected();
}

void DeviceManager::HandleDisconnected()
{
    if (instance_)
        instance_->onDisconnected();
}

void DeviceManager::HandleLine(const String &line)
{
    if (instance_)
        instance_->onLine(line);
}

void DeviceManager::HandleRaw(const uint8_t *data, size_t len)
{
    if (instance_)
        instance_->onRaw(data, len);
}

void DeviceManager::lock()
{
    if (mutex_)
        xSemaphoreTakeRecursive(mutex_, portMAX_DELAY);
}

void DeviceManager::unlock()
{
    if (mutex_)
        xSemaphoreGiveRecursive(mutex_);
}

DeviceManager::DeviceManager(UsbCdcHost &host)
    : host_(host)
{
    instance_ = this;
    mutex_ = xSemaphoreCreateRecursiveMutex();
}

DeviceManager::~DeviceManager()
{
    if (mutex_)
    {
        vSemaphoreDelete(mutex_);
        mutex_ = nullptr;
    }
    if (instance_ == this)
        instance_ = nullptr;
}

void DeviceManager::begin(uint16_t vid, uint16_t pid)
{
    instance_ = this;

    host_.setDeviceCallbacks(&DeviceManager::HandleConnected, &DeviceManager::HandleDisconnected);
    host_.setLineCallback(&DeviceManager::HandleLine);
    host_.setRawCallback(&DeviceManager::HandleRaw);

    host_.setVidPidFilter(vid, pid);

    ScopedLock lock(*this);
    enabled_ = false;
    device_id_logged_ = false;
    device_details_logged_ = false;
    awaiting_response_ = false;
    has_current_command_ = false;
    command_queue_.clear();
}

void DeviceManager::setRawLogging(bool enabled)
{
    ScopedLock lock(*this);
    raw_logging_enabled_ = enabled;
}

void DeviceManager::toggleRawLogging()
{
    ScopedLock lock(*this);
    raw_logging_enabled_ = !raw_logging_enabled_;
}

void DeviceManager::start()
{
    enable(true);
}

void DeviceManager::stop()
{
    enable(false);
}

void DeviceManager::enable(bool active)
{
    bool should_schedule = false;
    {
        ScopedLock lock(*this);
        if (enabled_ == active)
            return;

        enabled_ = active;

        command_queue_.clear();
        awaiting_response_ = false;
        has_current_command_ = false;
        device_id_logged_ = false;
        device_details_logged_ = false;
        current_command_ = PendingCommand{};

        should_schedule = enabled_;
    }

    if (should_schedule)
    {
        scheduleDeviceId(DEVICE_ID_INITIAL_DELAY_MS, true);
        processQueue();
    }
}

void DeviceManager::requestStats()
{
    {
        ScopedLock lock(*this);
        if (!enabled_ || !host_.isConnected() || !device_id_logged_)
            return;

        if (!isCommandPending("GET tubePulseCount") && (!awaiting_response_ || current_command_.command != "GET tubePulseCount"))
            enqueueCommand("GET tubePulseCount", CommandType::TubePulseCount, 0, false);
        if (!isCommandPending("GET tubeRate") && (!awaiting_response_ || current_command_.command != "GET tubeRate"))
            enqueueCommand("GET tubeRate", CommandType::TubeRate, 0, false);
        if (!isCommandPending("GET deviceBatteryVoltage") && (!awaiting_response_ || current_command_.command != "GET deviceBatteryVoltage"))
            enqueueCommand("GET deviceBatteryVoltage", CommandType::DeviceBatteryVoltage, 0, false);
    }

    processQueue();
}

void DeviceManager::requestRandomData()
{
    {
        ScopedLock lock(*this);
        if (!enabled_ || !host_.isConnected())
            return;
        if (!isCommandPending("GET randomData"))
            enqueueCommand("GET randomData", CommandType::RandomData, 0, true);
    }
    processQueue();
}

void DeviceManager::requestDataLog(const String &args)
{
    String cmd = "GET datalog";
    {
        ScopedLock lock(*this);
        if (!enabled_ || !host_.isConnected())
            return;
        if (args.length())
        {
            cmd += " ";
            cmd += args;
        }
        enqueueCommand(cmd, CommandType::DataLog, 0, true);
    }
    processQueue();
}

void DeviceManager::loop()
{
    if (!enabled_)
        return;

    bool connected = host_.isConnected();
    bool awaiting = false;
    unsigned long request_ms = 0;

    {
        ScopedLock lock(*this);
        if (!enabled_)
            return;

        if (!connected)
        {
            awaiting_response_ = false;
            has_current_command_ = false;
            command_queue_.clear();
            return;
        }

        awaiting = awaiting_response_;
        request_ms = last_request_ms_;
    }

    if (awaiting)
    {
        if ((millis() - request_ms) > DEVICE_ID_RESPONSE_TIMEOUT_MS)
            handleError();
        return;
    }

    processQueue();
}

void DeviceManager::onConnected()
{
    bool should_schedule = false;
    LineHandler handler;
    {
        ScopedLock lock(*this);
        device_id_logged_ = false;
        device_details_logged_ = false;
        command_queue_.clear();
        awaiting_response_ = false;
        has_current_command_ = false;
        current_command_ = PendingCommand{};

        should_schedule = enabled_;
        handler = line_handler_;
    }

    if (handler)
        handler(String("USB device CONNECTED"));

    if (should_schedule)
        scheduleDeviceId(DEVICE_ID_INITIAL_DELAY_MS, true);

    processQueue();
}

void DeviceManager::onDisconnected()
{
    bool should_schedule = false;
    LineHandler handler;
    {
        ScopedLock lock(*this);
        device_id_logged_ = false;
        device_details_logged_ = false;
        awaiting_response_ = false;
        has_current_command_ = false;
        command_queue_.clear();
        device_sensitivity_cpm_per_uSv_ = 0.0f;
        should_schedule = enabled_;
        handler = line_handler_;
    }

    if (should_schedule)
        scheduleDeviceId(DEVICE_ID_INITIAL_DELAY_MS, true);

    if (handler)
        handler(String("USB device DISCONNECTED"));
}

void DeviceManager::onLine(const String &line)
{
    ScopedLock lock(*this);

    if (verbose_logging_enabled_ && line_handler_)
        line_handler_(String("<- Line: ") + line);

    if (!awaiting_response_ || !has_current_command_)
        return;

    String trimmed = line;
    trimmed.trim();

    if (trimmed.equalsIgnoreCase(DEVICE_KEEPALIVE_LINE))
        return;

    if (trimmed.equalsIgnoreCase("ERROR"))
    {
        handleError();
        return;
    }

    switch (current_command_.type)
    {
    case CommandType::DeviceModel:
    case CommandType::DeviceFirmware:
    case CommandType::DeviceLocale:
    case CommandType::DeviceBatteryPercent:
    case CommandType::TubeDoseRate:
        handleSuccess();
        break;
    case CommandType::DeviceId:
    {
        if (!trimmed.startsWith("OK "))
            return;

        String payload = trimmed.substring(3);
        int firstSemi = payload.indexOf(';');
        int secondSemi = (firstSemi >= 0) ? payload.indexOf(';', firstSemi + 1) : -1;
        String deviceId;

        if (firstSemi > 0)
        {
            if (secondSemi > firstSemi && secondSemi > 0)
                deviceId = payload.substring(secondSemi + 1);
            else
                deviceId = payload.substring(firstSemi + 1);
            deviceId.trim();
            if (!device_id_logged_ && line_handler_)
                line_handler_(String("Device ID: ") + deviceId);
            device_id_logged_ = true;
            if (deviceId.length())
                emitResult(CommandType::DeviceId, deviceId, true);
        }

        if (!device_details_logged_)
        {
            String model;
            String firmware;
            String locale;

            if (firstSemi > 0)
            {
                model = payload.substring(0, firstSemi);
                model.trim();
            }

            if (secondSemi > firstSemi && secondSemi > 0)
            {
                String firmwareLocale = payload.substring(firstSemi + 1, secondSemi);
                firmwareLocale.trim();
                int slash = firmwareLocale.indexOf('/');
                if (slash >= 0)
                {
                    firmware = firmwareLocale.substring(0, slash);
                    firmware.trim();
                    locale = firmwareLocale.substring(slash + 1);
                    locale.trim();
                }
                else
                {
                    firmware = firmwareLocale;
                }
            }

            if (line_handler_)
            {
                if (model.length())
                    line_handler_(String("Device Model: ") + model);
                if (firmware.length())
                    line_handler_(String("Firmware: ") + firmware);
                if (locale.length())
                    line_handler_(String("Locale: ") + locale);
            }

            if (model.length())
                emitResult(CommandType::DeviceModel, model, true);
            if (firmware.length())
                emitResult(CommandType::DeviceFirmware, firmware, true);
            if (locale.length())
                emitResult(CommandType::DeviceLocale, locale, true);

            device_details_logged_ = true;
        }

        enqueueCommand("GET devicePower", CommandType::DevicePower, 0, true);
        enqueueCommand("GET deviceBatteryVoltage", CommandType::DeviceBatteryVoltage, 0, true);
        enqueueCommand("GET deviceTime", CommandType::DeviceTime, 0, true);
        enqueueCommand("GET deviceTimeZone", CommandType::DeviceTimeZone, 0, true);
        enqueueCommand("GET tubeTime", CommandType::TubeTime, 0, true);
        enqueueCommand("GET tubeSensitivity", CommandType::DeviceSensitivity, 0, true);
        enqueueCommand("GET tubeDeadTime", CommandType::TubeDeadTime, 0, true);
        enqueueCommand("GET tubeDeadTimeCompensation", CommandType::TubeDeadTimeCompensation, 0, true);
        enqueueCommand("GET tubeHVFrequency", CommandType::TubeHVFrequency, 0, true);
        enqueueCommand("GET tubeHVDutyCycle", CommandType::TubeHVDutyCycle, 0, true);

        handleSuccess();
        break;
    }
    case CommandType::DevicePower:
    {
        if (trimmed.startsWith("OK "))
        {
            String value = trimmed.substring(3);
            value.trim();
            if (line_handler_)
                line_handler_(String("Device Power: ") + (value == "1" ? "ON" : "OFF"));
            emitResult(CommandType::DevicePower, value, true);
        }
        handleSuccess();
        break;
    }
    case CommandType::DeviceBatteryVoltage:
    {
        if (trimmed.startsWith("OK "))
        {
            String value = trimmed.substring(3);
            value.trim();
            if (line_handler_)
                line_handler_(String("Battery Voltage: ") + value + " V");
            emitResult(CommandType::DeviceBatteryVoltage, value, true);

            float voltage = value.toFloat();
            float percent = (voltage - 3.0f) * (100.0f / (4.2f - 3.0f));
            if (percent < 0.0f)
                percent = 0.0f;
            if (percent > 100.0f)
                percent = 100.0f;
            uint8_t percentInt = static_cast<uint8_t>(percent + 0.5f);
            if (line_handler_)
                line_handler_(String("Battery Percent: ") + percentInt + " %");
            emitResult(CommandType::DeviceBatteryPercent, String(percentInt), true);
        }
        handleSuccess();
        break;
    }
    case CommandType::DeviceTime:
    {
        if (trimmed.startsWith("OK "))
        {
            String value = trimmed.substring(3);
            value.trim();
            if (line_handler_)
            {
                time_t ts = static_cast<time_t>(value.toInt());
                struct tm tm_info;
                gmtime_r(&ts, &tm_info);
                char buf[32];
                strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S UTC", &tm_info);
                line_handler_(String("Device Time: ") + buf + " (" + value + ")");
            }
            emitResult(CommandType::DeviceTime, value, true);
        }
        handleSuccess();
        break;
    }
    case CommandType::DeviceTimeZone:
    {
        if (trimmed.startsWith("OK "))
        {
            String zone = trimmed.substring(3);
            zone.trim();
            if (line_handler_)
                line_handler_(String("Device Time Zone: ") + zone);
            emitResult(CommandType::DeviceTimeZone, zone, true);
        }
        handleSuccess();
        break;
    }
    case CommandType::DeviceSensitivity:
    {
        if (trimmed.startsWith("OK "))
        {
            String sens = trimmed.substring(3);
            sens.trim();
            if (line_handler_)
                line_handler_(String("Tube Sensitivity: ") + sens + " cpm/µSv/h");
            emitResult(CommandType::DeviceSensitivity, sens, true);
            device_sensitivity_cpm_per_uSv_ = sens.toFloat();
        }
        handleSuccess();
        break;
    }
    case CommandType::TubeTime:
    {
        if (trimmed.startsWith("OK "))
        {
            String value = trimmed.substring(3);
            value.trim();
            if (line_handler_)
                line_handler_(String("Tube Lifetime: ") + value + " s");
            emitResult(CommandType::TubeTime, value, true);
        }
        handleSuccess();
        break;
    }
    case CommandType::TubePulseCount:
    {
        String value = trimmed.startsWith("OK ") ? trimmed.substring(3) : trimmed;
        value.trim();
        if (line_handler_)
            line_handler_(String("Tube Pulse Count: ") + value);
        emitResult(CommandType::TubePulseCount, value, true);
        handleSuccess();
        break;
    }
    case CommandType::TubeRate:
    {
        String value = trimmed.startsWith("OK ") ? trimmed.substring(3) : trimmed;
        value.trim();
        if (line_handler_)
            line_handler_(String("Tube Rate: ") + value + " cpm");
        emitResult(CommandType::TubeRate, value, true);

        float rate = value.toFloat();
        if (rate >= 0.0f)
        {
            float sensitivity = device_sensitivity_cpm_per_uSv_;
            if (sensitivity > 0.0f)
            {
                float dose = rate / sensitivity;
                if (line_handler_)
                    line_handler_(String("Dose Rate: ") + String(dose, 5) + " µSv/h");
                emitResult(CommandType::TubeDoseRate, String(dose, 5), true);
            }
        }
        handleSuccess();
        break;
    }
    case CommandType::TubeDeadTime:
    {
        if (trimmed.startsWith("OK "))
        {
            String value = trimmed.substring(3);
            value.trim();
            if (line_handler_)
                line_handler_(String("Tube Dead Time: ") + value + " s");
            emitResult(CommandType::TubeDeadTime, value, true);
        }
        handleSuccess();
        break;
    }
    case CommandType::TubeDeadTimeCompensation:
    {
        if (trimmed.startsWith("OK "))
        {
            String value = trimmed.substring(3);
            value.trim();
            if (line_handler_)
                line_handler_(String("Dead Time Compensation: ") + value + " s");
            emitResult(CommandType::TubeDeadTimeCompensation, value, true);
        }
        handleSuccess();
        break;
    }
    case CommandType::TubeHVFrequency:
    {
        if (trimmed.startsWith("OK "))
        {
            String value = trimmed.substring(3);
            value.trim();
            if (line_handler_)
                line_handler_(String("HV Frequency: ") + value + " Hz");
            emitResult(CommandType::TubeHVFrequency, value, true);
        }
        handleSuccess();
        break;
    }
    case CommandType::TubeHVDutyCycle:
    {
        if (trimmed.startsWith("OK "))
        {
            String value = trimmed.substring(3);
            value.trim();
            if (line_handler_)
                line_handler_(String("HV Duty Cycle: ") + value);
            emitResult(CommandType::TubeHVDutyCycle, value, true);
        }
        handleSuccess();
        break;
    }
    case CommandType::RandomData:
    {
        if (trimmed.startsWith("OK "))
        {
            String value = trimmed.substring(3);
            value.trim();
            if (line_handler_)
                line_handler_(String("Random Data: ") + value);
            emitResult(CommandType::RandomData, value, true);
        }
        handleSuccess();
        break;
    }
    case CommandType::DataLog:
    {
        if (trimmed.startsWith("OK "))
        {
            String value = trimmed.substring(3);
            if (line_handler_)
                line_handler_(String("Data Log: ") + value);
            emitResult(CommandType::DataLog, value, true);
        }
        handleSuccess();
        break;
    }
    case CommandType::Generic:
    {
        if (line_handler_)
            line_handler_(String("") + current_command_.command + " -> " + trimmed);
        handleSuccess();
        break;
    }
    }
}

void DeviceManager::onRaw(const uint8_t *data, size_t len)
{
    RawHandler handler = nullptr;
    {
        ScopedLock lock(*this);
        if (!raw_logging_enabled_)
            return;
        handler = raw_handler_;
    }

    if (handler)
        handler(data, len);
}

void DeviceManager::scheduleDeviceId(uint32_t delay_ms, bool announce)
{
    enqueueCommand("GET deviceId", CommandType::DeviceId, delay_ms, announce);
}

void DeviceManager::enqueueCommand(const String &cmd, CommandType type, uint32_t delay_ms, bool announce)
{
    PendingCommand entry{cmd, type, announce, 0, millis() + delay_ms};
    ScopedLock lock(*this);
    command_queue_.push_back(entry);
}

bool DeviceManager::isCommandPending(const String &cmd) const
{
    ScopedLock lock(const_cast<DeviceManager &>(*this));
    if (has_current_command_ && current_command_.command == cmd)
        return true;
    for (const auto &entry : command_queue_)
    {
        if (entry.command == cmd)
            return true;
    }
    return false;
}

void DeviceManager::processQueue()
{
    PendingCommand command_to_issue;
    bool should_issue = false;

    {
        ScopedLock lock(*this);
        if (awaiting_response_ || has_current_command_ || command_queue_.empty())
            return;

        unsigned long now = millis();
        for (auto it = command_queue_.begin(); it != command_queue_.end(); ++it)
        {
            if (static_cast<long>(now - it->ready_ms) >= 0)
            {
                current_command_ = *it;
                command_to_issue = current_command_;
                command_queue_.erase(it);
                has_current_command_ = true;
                awaiting_response_ = true;
                last_request_ms_ = millis();
                should_issue = true;
                break;
            }
        }
    }

    if (should_issue)
        issueCurrentCommand(command_to_issue);
}

void DeviceManager::issueCurrentCommand(const PendingCommand &command)
{
    if (!host_.isConnected())
    {
        ScopedLock lock(*this);
        awaiting_response_ = false;
        has_current_command_ = false;
        current_command_ = PendingCommand{};
        return;
    }

    if (command.announce && verbose_logging_enabled_ && line_handler_)
        line_handler_(String("-> Queue: ") + command.command);

    bool ok = true;
    if (command.type == CommandType::DeviceId)
    {
        ok = host_.sendCommand(command.command, true) &&
             host_.sendCommand(command.command + "\n", false) &&
             host_.sendCommand(command.command + "\r", false);
    }
    else
    {
        ok = host_.sendCommand(command.command, true);
    }

    if (!ok)
        handleError();
}

void DeviceManager::handleSuccess()
{
    {
        ScopedLock lock(*this);
        awaiting_response_ = false;
        has_current_command_ = false;
        current_command_ = PendingCommand{};
    }
    processQueue();
}

void DeviceManager::handleError()
{
    bool should_retry = false;
    bool retry_front = false;
    PendingCommand retry_cmd;
    CommandType failed_type = CommandType::Generic;
    String failed_command;
    LineHandler handler = nullptr;
    bool should_log_failure = false;

    {
        ScopedLock lock(*this);
        awaiting_response_ = false;

        if (!has_current_command_)
            return;

        failed_type = current_command_.type;
        failed_command = current_command_.command;

        if (current_command_.type == CommandType::DeviceId && current_command_.retry < DEVICE_ID_MAX_RETRY)
        {
            retry_cmd = current_command_;
            retry_cmd.retry++;
            retry_cmd.ready_ms = millis() + DEVICE_ID_RETRY_DELAY_MS;
            should_retry = true;
            retry_front = true;
        }
        else if ((current_command_.type == CommandType::TubePulseCount || current_command_.type == CommandType::TubeRate) &&
                 current_command_.retry < 1)
        {
            retry_cmd = current_command_;
            retry_cmd.retry++;
            retry_cmd.ready_ms = millis() + DEVICE_ID_RETRY_DELAY_MS;
            should_retry = true;
        }
        else if (line_handler_ &&
                 current_command_.type != CommandType::TubePulseCount &&
                 current_command_.type != CommandType::TubeRate)
        {
            handler = line_handler_;
            should_log_failure = true;
        }

        if (should_retry)
        {
            if (retry_front)
                command_queue_.insert(command_queue_.begin(), retry_cmd);
            else
                command_queue_.push_back(retry_cmd);
        }

        has_current_command_ = false;
        current_command_ = PendingCommand{};
    }

    if (should_log_failure && handler)
        handler(String("Command failed: ") + failed_command);

    emitResult(failed_type, String(), false);

    processQueue();
}

void DeviceManager::emitResult(CommandType type, const String &value, bool success)
{
    if (command_result_handler_)
    {
        command_result_handler_(type, value, success);
    }
}
