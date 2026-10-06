#pragma once

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include "Core/Common.h"
#include "Core/Future.h"
#include "Core/ServiceLocator.h"
#include "Interfaces/ITaskService.h"

namespace Elysium {

class TaskService : public Services::ITaskService {
   public:
    explicit TaskService(ServiceLocator&) {}

    ~TaskService() override {
        Shutdown();
    }

    void Initialize() override {
        shouldExit_ = false;
        workers_.clear();
        const unsigned count = WorkerCount();
        workers_.reserve(count);
        for (unsigned i = 0; i < count; i++) {
            workers_.emplace_back(&TaskService::WorkerLoop, this, i);
        }
    }

    void Shutdown() override {
        {
            std::lock_guard<std::mutex> lock(workMutex_);
            shouldExit_ = true;
        }
        workCondition_.notify_all();  // every worker has to see it, not just one

        for (std::thread& worker : workers_) {
            if (worker.joinable()) worker.join();
        }
        workers_.clear();
    }

    // Submit work to the pool. The task runs on whichever worker picks it up first;
    // pollCompleted is polled on the main thread during Update() to know when to
    // fire the caller's Future continuations (see ITaskService::Submit<T>).
    void SubmitRaw(std::function<void()> task, std::function<bool()> pollCompleted) override {
        {
            std::lock_guard<std::mutex> lock(workMutex_);
            workQueue_.push(std::move(task));
            pendingCount_++;
        }
        workCondition_.notify_one();

        {
            std::lock_guard<std::mutex> lock(pollMutex_);
            pollCallbacks_.push_back(std::move(pollCompleted));
        }
    }

    // Drains completed futures on the main thread, firing their Then() continuations
    void Update(float deltaTime) override {
        // Move callbacks out so we don't hold the lock while polling.
        // This prevents deadlock when a continuation calls Submit() re-entrantly
        // (e.g. sprite loading triggers sheet texture loads).
        std::vector<std::function<bool()>> callbacks;
        {
            std::lock_guard<std::mutex> lock(pollMutex_);
            callbacks = std::move(pollCallbacks_);
            pollCallbacks_.clear();
        }

        // Poll without holding the lock — continuations may call Submit()
        for (auto& poll : callbacks) {
            if (!poll()) {
                // Not yet resolved, put it back
                std::lock_guard<std::mutex> lock(pollMutex_);
                pollCallbacks_.push_back(std::move(poll));
            }
        }
    }

    bool IsIdle() const override {
        return pendingCount_.load() == 0;
    }

    void Render() override {}

   private:
    void WorkerLoop(unsigned index) {
        const std::string name = "Task Worker " + std::to_string(index);
        ProfileThread(name.c_str());
        while (true) {
            std::function<void()> task;

            {
                std::unique_lock<std::mutex> lock(workMutex_);
                workCondition_.wait(lock, [this] {
                    return !workQueue_.empty() || shouldExit_;
                });

                if (shouldExit_ && workQueue_.empty()) {
                    break;
                }

                if (!workQueue_.empty()) {
                    task = std::move(workQueue_.front());
                    workQueue_.pop();
                }
            }

            if (task) {
                ProfileN("Task");
                task();
                pendingCount_--;
            }
        }
    }

    // How many threads decode assets. Asset loads are CPU-bound (a sprite sheet is tens of
    // megapixels of PNG), so this scales nearly linearly; one core is left for the main thread,
    // which still has to finalize every load onto the GPU. Capped because past a handful of
    // threads the work is bound by disk and by the finalize queue rather than by cores.
    static unsigned WorkerCount() {
        const unsigned cores = std::thread::hardware_concurrency();
        if (cores <= 2) return 1;
        return std::min(cores - 1, 8u);
    }

    // Worker thread state
    std::vector<std::thread> workers_;
    std::mutex workMutex_;
    std::condition_variable workCondition_;
    std::queue<std::function<void()>> workQueue_;
    std::atomic<bool> shouldExit_{false};
    std::atomic<int> pendingCount_{0};

    // Main thread polling
    std::mutex pollMutex_;
    std::vector<std::function<bool()>> pollCallbacks_;
};

}  // namespace Elysium
