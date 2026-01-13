#pragma once

#include <string>
#include <thread>
#include <mutex>
#include <queue>
#include <atomic>
#include <condition_variable>
#include <functional>
#include "imgui.h"
#include "Service.h"
// #include "Common.h"

namespace Elysium {
    template <typename TaskType>
    class TaskService : public Service {
        static_assert(std::is_base_of_v<Task, TaskType>, "TaskType must inherit from Task");
        
    public:
        TaskService() : shouldExit_(false), isProcessing_(false), totalTasks_(0), processedTasks_(0) {}
        
        virtual ~TaskService() {
            Shutdown();
        }

        void Initialize() override {
            shouldExit_ = false;
            isProcessing_ = false;
            totalTasks_ = 0;
            processedTasks_ = 0;
            
            workerThread_ = std::thread(&TaskService::WorkerThreadFunction, this);
        }

        void Shutdown() override {
            shouldExit_ = true;
            
            // Wake up worker thread
            {
                std::lock_guard<std::mutex> lock(queueMutex_);
                taskCondition_.notify_one();
            }
            
            if (workerThread_.joinable()) {
                workerThread_.join();
            }
            
            // Clear remaining tasks
            std::lock_guard<std::mutex> lock(queueMutex_);
            while (!taskQueue_.empty()) {
                taskQueue_.pop();
            }
        }

        void SubmitTask(std::unique_ptr<TaskType> task) {
            {
                std::lock_guard<std::mutex> lock(queueMutex_);
                taskQueue_.push(std::move(task));
                totalTasks_++;
            }
            taskCondition_.notify_one();
        }

        bool IsProcessing() const {
            return isProcessing_.load();
        }

        bool IsComplete() const {
            return GetProgress() >= 1.0f;
        }

        float GetProgress() const {
            int total = totalTasks_.load();
            if (total == 0) return 1.0f;
            return static_cast<float>(processedTasks_.load()) / static_cast<float>(total);
        }

        int GetTotalTasks() const {
            return totalTasks_.load();
        }

        int GetProcessedTasks() const {
            return processedTasks_.load();
        }

        int GetQueuedTasks() const {
            std::lock_guard<std::mutex> lock(queueMutex_);
            return taskQueue_.size();
        }

        void ClearQueue() {
            std::lock_guard<std::mutex> lock(queueMutex_);
            while (!taskQueue_.empty()) {
                taskQueue_.pop();
            }
            totalTasks_ = 0;
            processedTasks_ = 0;
        }

        void Update(float deltaTime) override {
            // Can be overridden by derived classes for additional logic
        }

        void OnDebugDraw() override {
            
        }

    protected:
        // Override this for custom task processing logic
        virtual void OnTaskProcessed(TaskType* task) {}
        virtual void OnTaskFailed(TaskType* task, const std::exception& e) {}
        virtual void OnQueueEmpty() {}
        virtual void OnProcessingStarted() {}
        virtual void OnProcessingFinished() {}

    private:
        void WorkerThreadFunction() {
            // Profile;
            OnProcessingStarted();
            
            while (!shouldExit_) {
                std::unique_ptr<TaskType> task;
                
                // Wait for tasks or exit signal
                {
                    std::unique_lock<std::mutex> lock(queueMutex_);
                    taskCondition_.wait(lock, [this] { 
                        return !taskQueue_.empty() || shouldExit_; 
                    });
                    
                    if (shouldExit_) break;
                    
                    if (!taskQueue_.empty()) {
                        task = std::move(taskQueue_.front());
                        taskQueue_.pop();
                        isProcessing_ = true;
                    }
                }
                
                if (task) {
                    try {
                        task->Execute();
                        processedTasks_++;
                        OnTaskProcessed(task.get());
                    } catch (const std::exception& e) {
                        OnTaskFailed(task.get(), e);
                    }
                }
                
                // Check if queue is empty
                {
                    std::lock_guard<std::mutex> lock(queueMutex_);
                    if (taskQueue_.empty()) {
                        isProcessing_ = false;
                        OnQueueEmpty();
                    }
                }
            }
            
            isProcessing_ = false;
            OnProcessingFinished();
        }

        std::thread workerThread_;
        mutable std::mutex queueMutex_;
        std::condition_variable taskCondition_;
        std::queue<std::unique_ptr<TaskType>> taskQueue_;
        
        std::atomic<bool> shouldExit_;
        std::atomic<bool> isProcessing_;
        std::atomic<int> totalTasks_;
        std::atomic<int> processedTasks_;
    };
};