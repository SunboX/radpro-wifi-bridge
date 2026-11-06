// SPDX-FileCopyrightText: 2026 André Fiedler
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <array>
#include <utility>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "DeviceManager.h"

// USB responses arrive on the CDC task. PubSubClient belongs to the main loop.
class MqttCommandQueue
{
public:
    struct Result
    {
        DeviceManager::CommandType type = DeviceManager::CommandType::Generic;
        String value;
    };
    static constexpr size_t kCapacity = 32;

    MqttCommandQueue() : mutex_(xSemaphoreCreateMutex()) {}
    ~MqttCommandQueue() { if (mutex_) vSemaphoreDelete(mutex_); }
    MqttCommandQueue(const MqttCommandQueue &) = delete;
    MqttCommandQueue &operator=(const MqttCommandQueue &) = delete;

    void push(DeviceManager::CommandType type, const String &value)
    {
        if (!mutex_ || xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE)
            return;
        for (size_t i = 0; i < count_; ++i)
        {
            if (pending_[i].type == type)
            {
                pending_[i].value = value;
                xSemaphoreGive(mutex_);
                return;
            }
        }
        if (count_ < kCapacity)
        {
            pending_[count_].type = type;
            pending_[count_++].value = value;
        }
        xSemaphoreGive(mutex_);
    }

    bool pop(Result &result)
    {
        if (!mutex_ || xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE)
            return false;
        const bool haveResult = count_ > 0;
        if (haveResult)
        {
            result = std::move(pending_[0]);
            for (size_t i = 1; i < count_; ++i)
                pending_[i - 1] = std::move(pending_[i]);
            pending_[--count_] = Result{};
        }
        xSemaphoreGive(mutex_);
        return haveResult;
    }

private:
    SemaphoreHandle_t mutex_;
    std::array<Result, kCapacity> pending_;
    size_t count_ = 0;
};
