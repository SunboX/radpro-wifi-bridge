/*
 * SPDX-FileCopyrightText: 2026 André Fiedler
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "Arduino.h"
#include "freertos/semphr.h"

class UsbCdcHost
{
public:
    using DeviceCb = void (*)();
    using LineCb = void (*)(const String &);
    using RawCb = void (*)(const uint8_t *, size_t);

    void setDeviceCallbacks(DeviceCb on_connected, DeviceCb on_disconnected)
    {
        on_connected_ = on_connected;
        on_disconnected_ = on_disconnected;
    }

    void setLineCallback(LineCb on_line)
    {
        on_line_ = on_line;
    }

    void setRawCallback(RawCb on_raw)
    {
        on_raw_ = on_raw;
    }

    void setVidPidFilters(const std::vector<std::pair<uint16_t, uint16_t>> &filters)
    {
        filters_ = filters;
    }

    bool sendCommand(const String &cmd, bool append_crlf = true, uint32_t = 1000)
    {
        std::string text = cmd.c_str();
        if (append_crlf)
            text += "\r\n";
        sent_commands_.push_back(text);
        return true;
    }

    bool isConnected() const
    {
        return connected_;
    }

    bool hasObservedDevice() const
    {
        return observed_device_observed_ || connected_;
    }

    uint16_t connectedVid() const
    {
        return connected_vid_;
    }

    uint16_t connectedPid() const
    {
        return connected_pid_;
    }

    bool restart()
    {
        restarted_ = true;
        return true;
    }

    bool requestRestart()
    {
        return restart();
    }

    bool restartRunsInBackground() const
    {
        return true;
    }

    void simulateConnect(uint16_t vid = 0x0483, uint16_t pid = 0x5740)
    {
        observed_device_observed_ = true;
        connected_ = true;
        connected_vid_ = vid;
        connected_pid_ = pid;
        if (on_connected_)
            on_connected_();
    }

    void simulateDisconnect()
    {
        connected_ = false;
        if (on_disconnected_)
            on_disconnected_();
    }

    void simulateLine(const String &line)
    {
        if (on_line_)
            on_line_(line);
    }

    const std::vector<std::string> &sentCommands() const
    {
        return sent_commands_;
    }

    bool restarted() const
    {
        return restarted_;
    }

private:
    DeviceCb on_connected_ = nullptr;
    DeviceCb on_disconnected_ = nullptr;
    LineCb on_line_ = nullptr;
    RawCb on_raw_ = nullptr;
    std::vector<std::pair<uint16_t, uint16_t>> filters_;
    std::vector<std::string> sent_commands_;
    bool connected_ = false;
    bool observed_device_observed_ = false;
    bool restarted_ = false;
    uint16_t connected_vid_ = 0;
    uint16_t connected_pid_ = 0;
};
