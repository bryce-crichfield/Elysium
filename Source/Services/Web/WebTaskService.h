#pragma once

#include <chrono>
#include <deque>
#include <functional>
#include <vector>

#include "Core/Common.h"
#include "Core/ServiceLocator.h"
#include "Interfaces/ITaskService.h"

namespace Elysium {

// TaskService without threads, for the browser: a WebAssembly build without pthreads can't
// start any, and with them it needs cross-origin isolation from the server. Work runs on the
// main thread during Update, up to a frame budget, so loads still arrive over several frames
// and callers see the same Future flow as on desktop.
class WebTaskService : public Services::ITaskService {
   public:
    explicit WebTaskService(ServiceLocator&) {}

    void Initialize() override {}
    void Shutdown() override {
        queue_.clear();
        polls_.clear();
    }

    void SubmitRaw(std::function<void()> task, std::function<bool()> pollCompleted) override {
        queue_.push_back(std::move(task));
        polls_.push_back(std::move(pollCompleted));
    }

    void Update(float) override {
        Profile;
        // Only what was queued before this frame runs now: continuations below submit more,
        // and those wait for the next Update rather than stretching this one.
        const auto deadline = std::chrono::steady_clock::now() + FrameBudget;
        size_t count = queue_.size();
        while (count-- > 0) {
            std::function<void()> task = std::move(queue_.front());
            queue_.pop_front();
            task();
            if (std::chrono::steady_clock::now() >= deadline) break;
        }

        std::vector<std::function<bool()>> polls = std::move(polls_);
        polls_.clear();
        for (auto& poll : polls) {
            if (!poll()) polls_.push_back(std::move(poll));
        }
    }

    bool IsIdle() const override { return queue_.empty(); }

    void Render() override {}

   private:
    static constexpr std::chrono::milliseconds FrameBudget{8};

    std::deque<std::function<void()>> queue_;
    std::vector<std::function<bool()>> polls_;
};

}  // namespace Elysium
