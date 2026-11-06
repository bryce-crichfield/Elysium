#include "Services/Web/WebLogService.h"
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include "raylib.h"

namespace Elysium::Services {

namespace {

// raylib's TraceLog callback has no user-data slot; see LogService.
WebLogService* g_instance = nullptr;

void TraceLogCallback(int logLevel, const char* text, va_list args) {
    if (!g_instance) return;
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), text, args);
    g_instance->LogMessage(logLevel, std::string(buffer));
}

const char* LevelName(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::Error: return "ERROR";
    }
    return "UNKNOWN";
}

}  // namespace

WebLogService::WebLogService(ServiceLocator&) {
    logBuffer_.reserve(MaxLogBufferSize);
    g_instance = this;
    SetTraceLogCallback(TraceLogCallback);
    SetTraceLogLevel(LOG_DEBUG);
}

WebLogService::~WebLogService() {
    if (g_instance == this) g_instance = nullptr;
}

void WebLogService::LogMessage(int logLevel, const std::string& message) {
    // TraceLog lines from LOG_* are "[Topic] - Message"; raylib's own have no topic.
    std::string topic = "System";
    std::string text = message;
    if (message.size() > 3 && message[0] == '[') {
        const size_t close = message.find(']');
        const size_t dash = message.find(" - ", close);
        if (close != std::string::npos && close > 1 && dash != std::string::npos) {
            topic = message.substr(1, close - 1);
            text = message.substr(dash + 3);
        }
    }

    LogLevel level = LogLevel::INFO;
    if (logLevel == LOG_DEBUG) level = LogLevel::DEBUG;
    else if (logLevel == LOG_WARNING) level = LogLevel::WARNING;
    else if (logLevel >= LOG_ERROR) level = LogLevel::Error;

    Record(LogEntry(level, topic, text));
}

void WebLogService::LogMessage(LogLevel level, const std::string& topic, const std::string& message) {
    Record(LogEntry(level, topic, message));
}

void WebLogService::Record(const LogEntry& entry) {
    printf("[%s] [%s] %s\n", LevelName(entry.level), entry.topic.c_str(), entry.message.c_str());

    topics_.insert(entry.topic);
    if (logBuffer_.size() >= MaxLogBufferSize) logBuffer_.erase(logBuffer_.begin());
    logBuffer_.push_back(entry);
}

std::vector<std::string> WebLogService::GetAllTopics() const {
    std::vector<std::string> topics(topics_.begin(), topics_.end());
    std::sort(topics.begin(), topics.end());
    return topics;
}

std::string WebLogService::FormatTimestamp(const std::chrono::system_clock::time_point& timestamp) const {
    const std::time_t time = std::chrono::system_clock::to_time_t(timestamp);
    char buffer[16];
    std::strftime(buffer, sizeof(buffer), "%H:%M:%S", std::localtime(&time));
    return buffer;
}

std::string WebLogService::FormatLogEntry(const LogEntry& entry) const {
    return "[" + FormatTimestamp(entry.timestamp) + "] [" + LevelName(entry.level) + "] [" + entry.topic + "] " +
           entry.message;
}

}  // namespace Elysium::Services
