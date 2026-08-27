#pragma once

#include <chrono>
#include <string>
#include <vector>

// Undefine Windows ERROR macro to avoid conflict with LogLevel::Error
#ifdef ERROR
#undef ERROR
#endif

namespace Elysium::Services {

enum class LogLevel { DEBUG = 0, INFO = 1, WARNING = 2, Error = 3 };

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

// Query/persist log history for the editor (LogEditor). The LOG_* macros
// (Core/Log.h) are the write side and don't go through this interface —
// they're a dependency-free facade so Core/ never needs ILogService.
class ILogService {
   public:
    virtual ~ILogService() = default;

    virtual void LogMessage(int logLevel, const std::string& message) = 0;
    virtual void LogMessage(LogLevel level, const std::string& topic, const std::string& message) = 0;

    virtual const std::vector<LogEntry>& GetLogBuffer() const = 0;
    virtual std::vector<std::string> GetAllTopics() const = 0;
    virtual std::string FormatTimestamp(const std::chrono::system_clock::time_point& timestamp) const = 0;
    virtual std::string FormatLogEntry(const LogEntry& entry) const = 0;
};

}  // namespace Elysium::Services
