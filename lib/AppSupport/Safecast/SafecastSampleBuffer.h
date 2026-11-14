// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

// Caller serializes access. Only the selected unit needs averaging history;
// the latest reading of either unit remains available for a test upload.
class SafecastSampleBuffer {
public:
    static constexpr size_t kCapacity = 1024;
    struct Snapshot {
        bool collecting, doseUnit, hasCpm, hasDose;
        size_t count;
        uint32_t capacityDrops;
        float latestCpm, latestDose;
    };
    void configure(bool collecting, bool doseUnit, uint32_t windowMs) {
        if (!collecting || collecting_ != collecting || doseUnit_ != doseUnit || windowMs_ != windowMs)
            clearHistory();
        collecting_ = collecting;
        doseUnit_ = doseUnit;
        windowMs_ = windowMs;
    }
    void observe(bool doseUnit, float value, uint32_t now) {
        if (doseUnit) { latestDose_ = value; hasDose_ = true; }
        else { latestCpm_ = value; hasCpm_ = true; }
        if (!collecting_ || doseUnit != doseUnit_) return;
        prune(now);
        if (count_ == kCapacity) { pop(); ++capacityDrops_; }
        samples_[(head_ + count_) % kCapacity] = {now, value};
        ++count_;
        sum_ += value;
    }
    bool average(uint32_t now, float &value) {
        prune(now);
        if (!count_) return false;
        value = static_cast<float>(sum_ / count_);
        return true;
    }
    bool latest(bool doseUnit, float &value) const {
        if (doseUnit ? !hasDose_ : !hasCpm_) return false;
        value = doseUnit ? latestDose_ : latestCpm_;
        return true;
    }
    void clear() { clearHistory(); hasCpm_ = hasDose_ = false; }
    Snapshot snapshot() const {
        return {collecting_, doseUnit_, hasCpm_, hasDose_, count_, capacityDrops_, latestCpm_, latestDose_};
    }
private:
    struct Sample { uint32_t at; float value; };
    std::array<Sample, kCapacity> samples_{};
    size_t head_ = 0, count_ = 0;
    double sum_ = 0;
    uint32_t windowMs_ = 0, capacityDrops_ = 0;
    bool collecting_ = false, doseUnit_ = false, hasCpm_ = false, hasDose_ = false;
    float latestCpm_ = 0, latestDose_ = 0;
    void clearHistory() { head_ = count_ = 0; sum_ = 0; capacityDrops_ = 0; }
    void pop() {
        sum_ -= samples_[head_].value;
        head_ = (head_ + 1) % kCapacity;
        if (--count_ == 0) sum_ = 0;
    }
    void prune(uint32_t now) {
        while (count_ && static_cast<uint32_t>(now - samples_[head_].at) > windowMs_) pop();
    }
};
