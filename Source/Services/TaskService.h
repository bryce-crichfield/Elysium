#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "Core/Future.h"
#include "Service.h"

namespace Elysium {

class TaskService : public Service {
   public:
    TaskService(ServiceRegistry& registry) : Service(registry) {
        name_ = "TaskService";
    }

    ~TaskService() override {
        Shutdown();
    }

    void Initialize() override {
        shouldExit_ = false;
        workerThread_ = std::thread(&TaskService::WorkerLoop, this);
    }

    void Shutdown() override {
        {
            std::lock_guard<std::mutex> lock(workMutex_);
            shouldExit_ = true;
            workCondition_.notify_one();
        }

        if (workerThread_.joinable()) {
            workerThread_.join();
        }
    }

    // Submit work to the background thread, get a Future back.
    // The callable runs on the worker thread. The Future resolves there,
    // but continuations registered via Then() fire on the main thread during Update().
    template <typename T>
    Future<T> Submit(std::function<T()> work) {
        Future<T> future;

        auto task = [future, work = std::move(work)]() mutable {
            T result = work();
            future.Resolve(std::move(result));
        };

        {
            std::lock_guard<std::mutex> lock(workMutex_);
            workQueue_.push(std::move(task));
            pendingCount_++;
        }
        workCondition_.notify_one();

        // Track the future for polling on the main thread
        {
            std::lock_guard<std::mutex> lock(pollMutex_);
            pollCallbacks_.push_back([future]() mutable -> bool {
                return future.Poll();
            });
        }

        return future;
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

    bool IsIdle() const {
        return pendingCount_.load() == 0;
    }

    void Render() override {}

   private:
    void WorkerLoop() {
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
                task();
                pendingCount_--;
            }
        }
    }

    // Worker thread state
    std::thread workerThread_;
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
