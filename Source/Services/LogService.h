#pragma once

#include <atomic>
#include <chrono>
#include <fstream>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "Core/Log.h"  // LOG_* macros — re-exported here so existing includers don't need to change
#include "Core/ServiceLocator.h"
#include "Interfaces/ILogService.h"

namespace Elysium::Services {

class LogService : public ILogService {
   public:
    LogService(ServiceLocator& registry);
    ~LogService();

    void Initialize() override;
    void Initialize(const std::string& logFilePath = "logs/engine.log");
    void Shutdown() override;
    void Update(float deltaTime) override;

    void LogMessage(int logLevel, const std::string& message) override;
    void LogMessage(LogLevel level, const std::string& topic, const std::string& message) override;

    // Accessors for LogEditor
    const std::vector<LogEntry>& GetLogBuffer() const override { return logBuffer_; }
    std::vector<std::string> GetAllTopics() const override;
    std::string FormatTimestamp(const std::chrono::system_clock::time_point& timestamp) const override;
    std::string FormatLogEntry(const LogEntry& entry) const override;

   private:
    // Core state
    bool initialized_;
    std::atomic<bool> shouldStop_;
    // Set once the writer thread is running and owns console output; until then LogMessage
    // prints inline.
    std::atomic<bool> writerOwnsStdout_{false};

    // Log storage and threading
    std::vector<LogEntry> logBuffer_;
    std::queue<LogEntry> pendingLogs_;
    std::mutex logMutex_;
    std::thread writerThread_;
    std::ofstream logFile_;
    std::string logFilePath_;

    // Topic discovery
    mutable std::unordered_set<std::string> discoveredTopics_;

    // Constants
    static constexpr size_t MAX_LOG_BUFFER_SIZE = 1000;

    // Core methods
    void WriterThreadFunction();
    void DrainToSinks(std::queue<LogEntry>& batch);
    void WriteLogToFile(const LogEntry& entry);
    // Appends entry to `out` as one coloured console line. Batched by the writer thread and
    // written with a single fwrite per drain: a console write is a slow syscall (tens of ms
    // through a Windows console host), so doing one per log line on the calling thread stalls
    // whichever thread logged -- including every asset worker.
    void AppendStdoutLine(const LogEntry& entry, std::string& out) const;
    // Fallback for messages logged before Initialize() starts the writer thread, so early
    // config and startup errors still reach the console.
    void WriteLogToStdoutNow(const LogEntry& entry) const;

    // Utilities
    const char* GetLogLevelName(LogLevel level) const;
    const char* GetLogLevelColor(LogLevel level) const;
};

}  // namespace Elysium::Services
