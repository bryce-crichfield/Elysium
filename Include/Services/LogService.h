#pragma once

#include "Service.h"
#include "raylib.h"
#include <string>
#include <vector>
#include <fstream>
#include <mutex>
#include <thread>
#include <atomic>
#include <queue>
#include <chrono>
#include <unordered_map>
#include <unordered_set>

namespace Elysium::Services {

enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERROR = 3
};

struct LogEntry {
    LogLevel level;
    std::string topic;
    std::string message;
    std::chrono::system_clock::time_point timestamp;

    LogEntry(LogLevel lvl, const std::string& tpc, const std::string& msg)
        : level(lvl), topic(tpc), message(msg), timestamp(std::chrono::system_clock::now()) {}

    // Legacy constructor for compatibility with TraceLog (raylib internal logs)
    LogEntry(int lvl, const std::string& msg)
        : level(LogLevel::INFO), topic("System"), message(msg), timestamp(std::chrono::system_clock::now()) {}
};

class LogService : public Elysium::Service {
public:
    LogService();
    ~LogService();

    void Initialize() override;
    void Initialize(const std::string& logFilePath = "logs/engine.log");
    void Shutdown() override;

    // Standardized service logging methods
    static void LogInfo(const std::string& topic, const std::string& message);
    static void LogWarning(const std::string& topic, const std::string& message);
    static void LogError(const std::string& topic, const std::string& message);
    static void LogDebug(const std::string& topic, const std::string& message);

    // Formatted logging with printf-style arguments
    template<typename... Args>
    static void LogInfoF(const std::string& topic, const char* format, Args... args) {
        char buffer[1024];
        snprintf(buffer, sizeof(buffer), format, args...);
        LogInfo(topic, std::string(buffer));
    }

    template<typename... Args>
    static void LogWarningF(const std::string& topic, const char* format, Args... args) {
        char buffer[1024];
        snprintf(buffer, sizeof(buffer), format, args...);
        LogWarning(topic, std::string(buffer));
    }

    template<typename... Args>
    static void LogErrorF(const std::string& topic, const char* format, Args... args) {
        char buffer[1024];
        snprintf(buffer, sizeof(buffer), format, args...);
        LogError(topic, std::string(buffer));
    }

    template<typename... Args>
    static void LogDebugF(const std::string& topic, const char* format, Args... args) {
        char buffer[1024];
        snprintf(buffer, sizeof(buffer), format, args...);
        LogDebug(topic, std::string(buffer));
    }

    void Update(float deltaTime) override;
    void ImGui() override;

    void LogMessage(int logLevel, const std::string& message);
    void LogMessage(LogLevel level, const std::string& topic, const std::string& message);

    // Get all discovered topics for filtering
    std::vector<std::string> GetAllTopics() const;

private:
    // Core state
    bool initialized_;
    std::atomic<bool> shouldStop_;

    // Log storage and threading
    std::vector<LogEntry> logBuffer_;
    std::queue<LogEntry> pendingLogs_;
    std::mutex logMutex_;
    std::thread writerThread_;
    std::ofstream logFile_;
    std::string logFilePath_;

    // Topic discovery
    mutable std::unordered_set<std::string> discoveredTopics_;

    // UI state - filtering
    std::unordered_map<std::string, bool> topicFilters_;
    std::unordered_map<LogLevel, bool> levelFilters_;
    bool showFilterPanel_ = false;
    std::string searchFilter_ = "";

    // UI state - selection
    std::unordered_set<int> selectedLogIndices_;
    int dragStartIndex_ = -1;
    bool isDragging_ = false;

    // Constants
    static constexpr size_t MAX_LOG_BUFFER_SIZE = 1000;
    static constexpr size_t MAX_DISPLAY_LOGS = 500;

    // Core methods
    void WriterThreadFunction();
    void WriteLogToFile(const LogEntry& entry);
    void WriteLogToStdout(const LogEntry& entry);
    void InitializeFilters();

    // UI rendering
    void DrawHeader();
    void DrawFilterPanel();
    void DrawLevelFilters();
    void DrawTopicFilters();
    void DrawLogEntries();
    void HandleLogSelection(int logIndex, const std::vector<int>& visibleIndices);
    void DrawLogContextMenu(int logIndex, const LogEntry& entry, const std::string& fullLogText);

    // Filtering and utilities
    bool ShouldDisplayEntry(const LogEntry& entry) const;
    std::string FormatTimestamp(const std::chrono::system_clock::time_point& timestamp) const;
    std::string FormatLogEntry(const LogEntry& entry) const;
    const char* GetLogLevelName(LogLevel level) const;
    const char* GetLogLevelColor(LogLevel level) const;
    unsigned int GetImGuiColor(LogLevel level) const;
};

} // namespace Elysium::Services

// Convenience macros for consistent logging
#define LOG_INFO(topic, message) Elysium::Services::LogService::LogInfo(topic, message)
#define LOG_WARNING(topic, message) Elysium::Services::LogService::LogWarning(topic, message)
#define LOG_ERROR(topic, message) Elysium::Services::LogService::LogError(topic, message)
#define LOG_DEBUG(topic, message) Elysium::Services::LogService::LogDebug(topic, message)

#define LOG_INFOF(topic, format, ...) Elysium::Services::LogService::LogInfoF(topic, format, __VA_ARGS__)
#define LOG_WARNINGF(topic, format, ...) Elysium::Services::LogService::LogWarningF(topic, format, __VA_ARGS__)
#define LOG_ERRORF(topic, format, ...) Elysium::Services::LogService::LogErrorF(topic, format, __VA_ARGS__)
#define LOG_DEBUGF(topic, format, ...) Elysium::Services::LogService::LogDebugF(topic, format, __VA_ARGS__)
