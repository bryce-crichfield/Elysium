#pragma once

#include <cstdio>
#include <string>

// Dependency-free logging entry points. These forward to raylib's TraceLog
// (routed to LogService::LogMessage via CustomTraceLogCallback at startup) —
// they don't touch LogService directly so Core/ doesn't need to depend on
// Services/. LogService itself remains the place to query/persist log history
// (see ILogService) — these are just the write-side macros used everywhere.
namespace Elysium::Log {

void Info(const std::string& topic, const std::string& message);
void Warning(const std::string& topic, const std::string& message);
void Error(const std::string& topic, const std::string& message);
void Debug(const std::string& topic, const std::string& message);

template <typename... Args>
void InfoF(const std::string& topic, const char* format, Args... args) {
    char buffer[1024];
    snprintf(buffer, sizeof(buffer), format, args...);
    Info(topic, std::string(buffer));
}

template <typename... Args>
void WarningF(const std::string& topic, const char* format, Args... args) {
    char buffer[1024];
    snprintf(buffer, sizeof(buffer), format, args...);
    Warning(topic, std::string(buffer));
}

template <typename... Args>
void ErrorF(const std::string& topic, const char* format, Args... args) {
    char buffer[1024];
    snprintf(buffer, sizeof(buffer), format, args...);
    Error(topic, std::string(buffer));
}

template <typename... Args>
void DebugF(const std::string& topic, const char* format, Args... args) {
    char buffer[1024];
    snprintf(buffer, sizeof(buffer), format, args...);
    Debug(topic, std::string(buffer));
}

}  // namespace Elysium::Log

#define LOG_INFO(topic, message) Elysium::Log::Info(topic, message)
#define LOG_WARNING(topic, message) Elysium::Log::Warning(topic, message)
#define LOG_ERROR(topic, message) Elysium::Log::Error(topic, message)
#define LOG_DEBUG(topic, message) Elysium::Log::Debug(topic, message)

#define LOG_INFOF(topic, format, ...) Elysium::Log::InfoF(topic, format, __VA_ARGS__)
#define LOG_WARNINGF(topic, format, ...) Elysium::Log::WarningF(topic, format, __VA_ARGS__)
#define LOG_ERRORF(topic, format, ...) Elysium::Log::ErrorF(topic, format, __VA_ARGS__)
#define LOG_DEBUGF(topic, format, ...) Elysium::Log::DebugF(topic, format, __VA_ARGS__)
