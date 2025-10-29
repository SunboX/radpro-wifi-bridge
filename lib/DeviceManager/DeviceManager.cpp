#include "DeviceManager.h"

namespace
{
    constexpr uint32_t DEVICE_ID_INITIAL_DELAY_MS = 2500;
    constexpr uint32_t DEVICE_ID_RETRY_DELAY_MS = 1500;
    constexpr uint32_t DEVICE_ID_RESPONSE_TIMEOUT_MS = 3000;
    constexpr uint8_t DEVICE_ID_MAX_RETRY = 4;
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
    retry_count_ = 0;
    request_issued_ = false;
    awaiting_response_ = false;
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

    if (enabled_)
    {
        device_id_logged_ = false;
        device_details_logged_ = false;
        retry_count_ = 0;
        scheduleRequest(DEVICE_ID_INITIAL_DELAY_MS, true);
    }
    else
    {
        awaiting_response_ = false;
        request_issued_ = false;
    }
}

void DeviceManager::loop()
{
    if (!enabled_)
        return;

    if (!host_.isConnected())
    {
        awaiting_response_ = false;
        return;
    }

    if (device_id_logged_)
        return;

    unsigned long now = millis();

    if (!request_issued_ && static_cast<long>(now - request_time_ms_) >= 0)
    {
        issueRequest(announce_next_);
        announce_next_ = false;
    }
    else if (awaiting_response_ && (now - last_request_ms_) > DEVICE_ID_RESPONSE_TIMEOUT_MS)
    {
        awaiting_response_ = false;
        if (retry_count_ < DEVICE_ID_MAX_RETRY)
        {
            retry_count_++;
            request_issued_ = false;
            request_time_ms_ = now + DEVICE_ID_RETRY_DELAY_MS;
            announce_next_ = true;
        }
        else
        {
            request_issued_ = true;
        }
    }
}

void DeviceManager::onConnected()
{
    device_id_logged_ = false;
    device_details_logged_ = false;
    retry_count_ = 0;
    if (enabled_)
        scheduleRequest(DEVICE_ID_INITIAL_DELAY_MS, true);
    if (line_handler_)
        line_handler_(String("[main] USB device CONNECTED"));
}

void DeviceManager::onDisconnected()
{
    device_id_logged_ = false;
    device_details_logged_ = false;
    retry_count_ = 0;
    if (enabled_)
        scheduleRequest(DEVICE_ID_INITIAL_DELAY_MS, true);
    if (line_handler_)
        line_handler_(String("[main] USB device DISCONNECTED"));
}

void DeviceManager::onLine(const String &line)
{
    if (line_handler_)
        line_handler_(String("[main] <- Line: ") + line);

    if (line.startsWith("OK "))
    {
        int deviceIdIndex = line.lastIndexOf(';');
        if (deviceIdIndex >= 0 && deviceIdIndex + 1 < line.length())
        {
            String deviceId = line.substring(deviceIdIndex + 1);
            if (!device_id_logged_ && line_handler_)
                line_handler_(String("[main] Device ID: ") + deviceId);
            device_id_logged_ = true;
        }
        if (!device_details_logged_)
        {
            String payload = line.substring(3); // remove "OK "
            int firstSemi = payload.indexOf(';');
            int secondSemi = (firstSemi >= 0) ? payload.indexOf(';', firstSemi + 1) : -1;

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
                    firmware.trim();
                }
            }

            if (line_handler_)
            {
                if (model.length())
                    line_handler_(String("[main] Device Model: ") + model);
                if (firmware.length())
                    line_handler_(String("[main] Firmware: ") + firmware);
                if (locale.length())
                    line_handler_(String("[main] Locale: ") + locale);
            }
            device_details_logged_ = true;
        }
        awaiting_response_ = false;
        request_issued_ = true;
        retry_count_ = 0;
        return;
    }

    if (line.equalsIgnoreCase("ERROR"))
    {
        awaiting_response_ = false;
        if (retry_count_ < DEVICE_ID_MAX_RETRY)
        {
            retry_count_++;
            request_issued_ = false;
            request_time_ms_ = millis() + DEVICE_ID_RETRY_DELAY_MS;
            announce_next_ = true;
        }
    }
}

void DeviceManager::onRaw(const uint8_t *data, size_t len)
{
    if (!raw_logging_enabled_ || !raw_handler_)
        return;
    raw_handler_(data, len);
}

void DeviceManager::scheduleRequest(uint32_t delay_ms, bool announce)
{
    if (device_id_logged_ || !enabled_)
        return;
    request_issued_ = false;
    awaiting_response_ = false;
    request_time_ms_ = millis() + delay_ms;
    announce_next_ = announce;
}

void DeviceManager::issueRequest(bool announce)
{
    if (!host_.isConnected())
        return;

    if (announce && line_handler_)
        line_handler_("[main] -> Queue: GET deviceId");

    host_.sendCommand("GET deviceId", true);
    host_.sendCommand("GET deviceId\n", false);
    host_.sendCommand("GET deviceId\r", false);

    request_issued_ = true;
    awaiting_response_ = true;
    last_request_ms_ = millis();
}
