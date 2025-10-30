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

DeviceManager::DeviceManager(UsbCdcHost &host)
    : host_(host)
{
    instance_ = this;
}

void DeviceManager::begin(uint16_t vid, uint16_t pid)
{
    instance_ = this;

    host_.setDeviceCallbacks(&DeviceManager::HandleConnected, &DeviceManager::HandleDisconnected);
    host_.setLineCallback(&DeviceManager::HandleLine);
    host_.setRawCallback(&DeviceManager::HandleRaw);

    host_.setVidPidFilter(vid, pid);

    enabled_ = false;
    device_id_logged_ = false;
    device_details_logged_ = false;
    awaiting_response_ = false;
    has_current_command_ = false;
    command_queue_.clear();
}

void DeviceManager::setRawLogging(bool enabled)
{
    raw_logging_enabled_ = enabled;
}

void DeviceManager::toggleRawLogging()
{
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
    if (enabled_ == active)
        return;

    enabled_ = active;

    command_queue_.clear();
    awaiting_response_ = false;
    has_current_command_ = false;
    device_id_logged_ = false;
    device_details_logged_ = false;
    current_command_ = PendingCommand{};

    if (enabled_)
    {
        scheduleDeviceId(DEVICE_ID_INITIAL_DELAY_MS, true);
        processQueue();
    }
}

void DeviceManager::requestStats()
{
    if (!enabled_ || !host_.isConnected() || !device_id_logged_)
        return;

    if (!isCommandPending("GET tubePulseCount") && (!awaiting_response_ || current_command_.command != "GET tubePulseCount"))
        enqueueCommand("GET tubePulseCount", CommandType::TubePulseCount, 0, false);
    if (!isCommandPending("GET tubeRate") && (!awaiting_response_ || current_command_.command != "GET tubeRate"))
        enqueueCommand("GET tubeRate", CommandType::TubeRate, 0, false);

    processQueue();
}

void DeviceManager::requestRandomData()
{
    if (!enabled_ || !host_.isConnected())
        return;
    if (!isCommandPending("GET randomData"))
        enqueueCommand("GET randomData", CommandType::RandomData, 0, true);
    processQueue();
}

void DeviceManager::requestDataLog(const String &args)
{
    if (!enabled_ || !host_.isConnected())
        return;
    String cmd = "GET datalog";
    if (args.length())
    {
        cmd += " ";
        cmd += args;
    }
    enqueueCommand(cmd, CommandType::DataLog, 0, true);
    processQueue();
}

void DeviceManager::loop()
{
    if (!enabled_)
        return;

    if (!host_.isConnected())
    {
        awaiting_response_ = false;
        has_current_command_ = false;
        command_queue_.clear();
        return;
    }

    if (awaiting_response_)
    {
        if ((millis() - last_request_ms_) > DEVICE_ID_RESPONSE_TIMEOUT_MS)
            handleError();
        return;
    }

    processQueue();
}

void DeviceManager::onConnected()
{
    device_id_logged_ = false;
    device_details_logged_ = false;
    command_queue_.clear();
    awaiting_response_ = false;
    has_current_command_ = false;
    current_command_ = PendingCommand{};

    if (enabled_)
        scheduleDeviceId(DEVICE_ID_INITIAL_DELAY_MS, true);

    if (line_handler_)
        line_handler_(String("USB device CONNECTED"));

    processQueue();
}

void DeviceManager::onDisconnected()
{
    device_id_logged_ = false;
    device_details_logged_ = false;
    awaiting_response_ = false;
    has_current_command_ = false;
    command_queue_.clear();

    if (enabled_)
        scheduleDeviceId(DEVICE_ID_INITIAL_DELAY_MS, true);

    if (line_handler_)
        line_handler_(String("USB device DISCONNECTED"));
}

void DeviceManager::onLine(const String &line)
{
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
    case CommandType::DeviceId:
    {
        if (!trimmed.startsWith("OK "))
            return;

        String payload = trimmed.substring(3);
        int firstSemi = payload.indexOf(';');
        int secondSemi = (firstSemi >= 0) ? payload.indexOf(';', firstSemi + 1) : -1;

        if (firstSemi > 0)
        {
            String deviceId;
            if (secondSemi > firstSemi && secondSemi > 0)
                deviceId = payload.substring(secondSemi + 1);
            else
                deviceId = payload.substring(firstSemi + 1);
            deviceId.trim();
            if (!device_id_logged_ && line_handler_)
                line_handler_(String("Device ID: ") + deviceId);
            device_id_logged_ = true;
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
        if (trimmed.startsWith("OK ") && line_handler_)
        {
            String value = trimmed.substring(3);
            value.trim();
            line_handler_(String("Device Power: ") + (value == "1" ? "ON" : "OFF"));
        }
        handleSuccess();
        break;
    }
    case CommandType::DeviceBatteryVoltage:
    {
        if (trimmed.startsWith("OK ") && line_handler_)
        {
            String value = trimmed.substring(3);
            value.trim();
            line_handler_(String("Battery Voltage: ") + value + " V");
        }
        handleSuccess();
        break;
    }
    case CommandType::DeviceTime:
    {
        if (trimmed.startsWith("OK ") && line_handler_)
        {
            String value = trimmed.substring(3);
            value.trim();
            time_t ts = static_cast<time_t>(value.toInt());
            struct tm tm_info;
            gmtime_r(&ts, &tm_info);
            char buf[32];
            strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S UTC", &tm_info);
            line_handler_(String("Device Time: ") + buf + " (" + value + ")");
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
        }
        handleSuccess();
        break;
    }
    case CommandType::TubePulseCount:
    {
        if (!trimmed.startsWith("OK "))
            return;

        if (line_handler_)
            line_handler_(current_command_.command + " -> " + trimmed);
        handleSuccess();
        break;
    }
    case CommandType::TubeRate:
    {
        if (!trimmed.startsWith("OK "))
            return;

        if (line_handler_)
            line_handler_(current_command_.command + " -> " + trimmed);
        handleSuccess();
        break;
    }
    case CommandType::TubeDeadTime:
    {
        if (trimmed.startsWith("OK ") && line_handler_)
        {
            String value = trimmed.substring(3);
            value.trim();
            line_handler_(String("Tube Dead Time: ") + value + " s");
        }
        handleSuccess();
        break;
    }
    case CommandType::TubeDeadTimeCompensation:
    {
        if (trimmed.startsWith("OK ") && line_handler_)
        {
            String value = trimmed.substring(3);
            value.trim();
            line_handler_(String("Dead Time Compensation: ") + value + " s");
        }
        handleSuccess();
        break;
    }
    case CommandType::TubeHVFrequency:
    {
        if (trimmed.startsWith("OK ") && line_handler_)
        {
            String value = trimmed.substring(3);
            value.trim();
            line_handler_(String("HV Frequency: ") + value + " Hz");
        }
        handleSuccess();
        break;
    }
    case CommandType::TubeHVDutyCycle:
    {
        if (trimmed.startsWith("OK ") && line_handler_)
        {
            String value = trimmed.substring(3);
            value.trim();
            line_handler_(String("HV Duty Cycle: ") + value);
        }
        handleSuccess();
        break;
    }
    case CommandType::RandomData:
    {
        if (trimmed.startsWith("OK ") && line_handler_)
        {
            String value = trimmed.substring(3);
            value.trim();
            line_handler_(String("Random Data: ") + value);
        }
        handleSuccess();
        break;
    }
    case CommandType::DataLog:
    {
        if (trimmed.startsWith("OK ") && line_handler_)
        {
            String value = trimmed.substring(3);
            line_handler_(String("Data Log: ") + value);
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
    if (!raw_logging_enabled_ || !raw_handler_)
        return;
    raw_handler_(data, len);
}

void DeviceManager::scheduleDeviceId(uint32_t delay_ms, bool announce)
{
    enqueueCommand("GET deviceId", CommandType::DeviceId, delay_ms, announce);
}

void DeviceManager::enqueueCommand(const String &cmd, CommandType type, uint32_t delay_ms, bool announce)
{
    PendingCommand entry{cmd, type, announce, 0, millis() + delay_ms};
    command_queue_.push_back(entry);
}

bool DeviceManager::isCommandPending(const String &cmd) const
{
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
    if (awaiting_response_ || has_current_command_)
        return;

    if (command_queue_.empty())
        return;

    unsigned long now = millis();
    for (auto it = command_queue_.begin(); it != command_queue_.end(); ++it)
    {
        if (static_cast<long>(now - it->ready_ms) >= 0)
        {
            current_command_ = *it;
            command_queue_.erase(it);
            has_current_command_ = true;
            issueCurrentCommand();
            break;
        }
    }
}

void DeviceManager::issueCurrentCommand()
{
    if (!has_current_command_ || !host_.isConnected())
        return;

    if (current_command_.announce && verbose_logging_enabled_ && line_handler_)
        line_handler_(String("-> Queue: ") + current_command_.command);

    if (current_command_.type == CommandType::DeviceId)
    {
        host_.sendCommand(current_command_.command, true);
        host_.sendCommand(current_command_.command + "\n", false);
        host_.sendCommand(current_command_.command + "\r", false);
    }
    else
    {
        host_.sendCommand(current_command_.command, true);
    }

    awaiting_response_ = true;
    last_request_ms_ = millis();
}

void DeviceManager::handleSuccess()
{
    awaiting_response_ = false;
    has_current_command_ = false;
    current_command_ = PendingCommand{};
    processQueue();
}

void DeviceManager::handleError()
{
    awaiting_response_ = false;

    if (!has_current_command_)
        return;

    if (current_command_.type == CommandType::DeviceId && current_command_.retry < DEVICE_ID_MAX_RETRY)
    {
        PendingCommand retry = current_command_;
        retry.retry++;
        retry.ready_ms = millis() + DEVICE_ID_RETRY_DELAY_MS;
        command_queue_.insert(command_queue_.begin(), retry);
    }
    else if ((current_command_.type == CommandType::TubePulseCount || current_command_.type == CommandType::TubeRate) &&
             current_command_.retry < 1)
    {
        PendingCommand retry = current_command_;
        retry.retry++;
        retry.ready_ms = millis() + DEVICE_ID_RETRY_DELAY_MS;
        command_queue_.push_back(retry);
    }
    else if (line_handler_ &&
             current_command_.type != CommandType::TubePulseCount &&
             current_command_.type != CommandType::TubeRate)
    {
        line_handler_(String("Command failed: ") + current_command_.command);
    }

    has_current_command_ = false;
    current_command_ = PendingCommand{};
    processQueue();
}
