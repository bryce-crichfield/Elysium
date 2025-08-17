#pragma once

#include "raylib.h"
#include <string>
#include <vector>
#include <fstream>
#include <mutex>
#include <thread>
#include <atomic>
#include <queue>
#include <chrono>

namespace Elysium::Services {

struct LogEntry {
    int level;
    std::string message;
    std::chrono::system_clock::time_point timestamp;

    LogEntry(int lvl, const std::string& msg)
        : level(lvl), message(msg), timestamp(std::chrono::system_clock::now()) {}
};

class LogService {
public:
    LogService();
    ~LogService();

    void Initialize(const std::string& logFilePath = "logs/engine.log");
    void Shutdown();

    // Standardized service logging methods
    static void LogInfo(const std::string& service, const std::string& message);
    static void LogWarning(const std::string& service, const std::string& message);
    static void LogError(const std::string& service, const std::string& message);

    // Section headers for major operations
    static void LogSectionStart(const std::string& section);
    static void LogSectionEnd(const std::string& section);

    // Formatted logging with printf-style arguments
    template<typename... Args>
    static void LogInfoF(const std::string& service, const char* format, Args... args) {
        char buffer[1024];
        snprintf(buffer, sizeof(buffer), format, args...);
        LogInfo(service, std::string(buffer));
    }

    template<typename... Args>
    static void LogWarningF(const std::string& service, const char* format, Args... args) {
        char buffer[1024];
        snprintf(buffer, sizeof(buffer), format, args...);
        LogWarning(service, std::string(buffer));
    }

    template<typename... Args>
    static void LogErrorF(const std::string& service, const char* format, Args... args) {
        char buffer[1024];
        snprintf(buffer, sizeof(buffer), format, args...);
        LogError(service, std::string(buffer));
    }

    void Update(float deltaTime);
    void Draw();

    void SetVisible(bool visible) { isVisible_ = visible; }
    bool IsVisible() const { return isVisible_; }
    void ToggleVisibility() { isVisible_ = !isVisible_; }

    void LogMessage(int logLevel, const std::string& message);

private:
    bool isVisible_;
    bool initialized_;

    std::vector<LogEntry> logBuffer_;
    std::queue<LogEntry> pendingLogs_;
    std::mutex logMutex_;

    std::ofstream logFile_;
    std::string logFilePath_;

    std::thread writerThread_;
    std::atomic<bool> shouldStop_;

    static constexpr size_t MAX_LOG_BUFFER_SIZE = 1000;
    static constexpr size_t MAX_DISPLAY_LOGS = 500;

    void WriterThreadFunction();
    void WriteLogToFile(const LogEntry& entry);
    void WriteLogToStdout(const LogEntry& entry);

    const char* GetLogLevelName(int level) const;
    const char* GetLogLevelColor(int level) const;
    unsigned int GetImGuiColor(int level) const;

    std::string FormatTimestamp(const std::chrono::system_clock::time_point& timestamp) const;
};

} // namespace Elysium::Services

// Convenience macros for consistent logging
#define LOG_INFO(service, message) Elysium::Services::LogService::LogInfo(service, message)
#define LOG_WARNING(service, message) Elysium::Services::LogService::LogWarning(service, message)
#define LOG_ERROR(service, message) Elysium::Services::LogService::LogError(service, message)

#define LOG_INFOF(service, format, ...) Elysium::Services::LogService::LogInfoF(service, format, __VA_ARGS__)
#define LOG_WARNINGF(service, format, ...) Elysium::Services::LogService::LogWarningF(service, format, __VA_ARGS__)
#define LOG_ERRORF(service, format, ...) Elysium::Services::LogService::LogErrorF(service, format, __VA_ARGS__)

#define LOG_SECTION_START(section) Elysium::Services::LogService::LogSectionStart(section)
#define LOG_SECTION_END(section) Elysium::Services::LogService::LogSectionEnd(section)
