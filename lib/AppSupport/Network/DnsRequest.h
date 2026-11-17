// SPDX-FileCopyrightText: 2026 André Fiedler
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <atomic>
#include <cstdint>
#include <cstring>

namespace BridgeDns
{
// One bounded request, owned by the resolver rather than the caller's stack.
// An abandoned request cannot be reused until its worker has finished.
class Request
{
public:
    enum class State { Idle, Queued, Running, Complete, Abandoned };
    bool start(const char *name)
    {
        if (state_.load() != State::Idle || !name || !name[0] || std::strlen(name) >= sizeof(name_))
            return false;
        std::strcpy(name_, name);
        state_.store(State::Queued);
        return true;
    }
    bool workerStart()
    {
        State expected = State::Queued;
        if (state_.compare_exchange_strong(expected, State::Running))
            return true;
        if (expected == State::Abandoned)
            state_.store(State::Idle);
        return false;
    }
    const char *name() const { return name_; }
    bool workerFinish(bool success, uint32_t address)
    {
        success_ = success;
        address_ = address;
        State expected = State::Running;
        if (state_.compare_exchange_strong(expected, State::Complete))
            return true;
        // Only the worker releases a request that outlived its caller.
        state_.store(State::Idle);
        return false;
    }
    bool complete() const { return state_.load() == State::Complete; }
    bool idle() const { return state_.load() == State::Idle; }
    bool take(uint32_t &address)
    {
        const bool success = success_;
        address = success ? address_ : 0;
        state_.store(State::Idle);
        return success;
    }
    void abandon()
    {
        State state = state_.load();
        while (state == State::Queued || state == State::Running)
        {
            if (state_.compare_exchange_weak(state, State::Abandoned))
                return;
        }
        if (state == State::Complete)
            state_.store(State::Idle);
    }
private:
    std::atomic<State> state_{State::Idle};
    char name_[256] = {};
    uint32_t address_ = 0;
    bool success_ = false;
};
} // namespace BridgeDns
