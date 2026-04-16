#pragma once

enum esp_log_level_t
{
    ESP_LOG_NONE = 0,
    ESP_LOG_ERROR,
    ESP_LOG_WARN,
    ESP_LOG_INFO
};

inline void esp_log_level_set(const char *, esp_log_level_t)
{
}
