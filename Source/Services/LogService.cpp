#include "Services/LogService.h"
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include "Core/Common.h"
#include "imgui.h"

namespace Elysium::Services {

LogService::LogService(ServiceRegistry& registry)
    : Service(registry), initialized_(false), shouldStop_(false) {
    name_ = "LogService";
    logBuffer_.reserve(MAX_LOG_BUFFER_SIZE);
}

LogService::~LogService() {
    Shutdown();
}

void LogService::Initialize() {
    Initialize("logs/engine.log");
}

void LogService::Initialize(const std::string& logFilePath) {
    if (initialized_)
        return;

    logFilePath_ = logFilePath;

    std::filesystem::path logPath(logFilePath_);
    std::filesystem::create_directories(logPath.parent_path());

    logFile_.open(logFilePath_, std::ios::out | std::ios::app);
    if (!logFile_.is_open()) {
        printf("[ERROR] Failed to open log file: %s\n", logFilePath_.c_str());
    }

    shouldStop_ = false;
    writerThread_ = std::thread(&LogService::WriterThreadFunction, this);

    initialized_ = true;
}

void LogService::Shutdown() {
    if (!initialized_)
        return;

    shouldStop_ = true;
    if (writerThread_.joinable()) {
        writerThread_.join();
    }

    if (logFile_.is_open()) {
        logFile_.close();
    }

    initialized_ = false;
}

void LogService::Update(float deltaTime) {
    Profile;
    std::lock_guard<std::mutex> lock(logMutex_);

    while (!pendingLogs_.empty()) {
        logBuffer_.push_back(pendingLogs_.front());
        pendingLogs_.pop();

        if (logBuffer_.size() > MAX_LOG_BUFFER_SIZE) {
            logBuffer_.erase(logBuffer_.begin());
        }
    }
}


void LogService::LogMessage(int logLevel, const std::string& message) {
    // Parse topic and message from TraceLog format: "[Topic] - Message"
    std::string topic = "System";
    std::string cleanMessage = message;

    if (message.length() > 3 && message[0] == '[') {
        size_t closeBracket = message.find(']');
        if (closeBracket != std::string::npos && closeBracket > 1) {
            topic = message.substr(1, closeBracket - 1);
            size_t dashPos = message.find(" - ", closeBracket);
            if (dashPos != std::string::npos) {
                cleanMessage = message.substr(dashPos + 3);
            }
        }
    }

    // Convert raylib log level to our LogLevel enum
    LogLevel level;
    switch (logLevel) {
        case 2:
            level = LogLevel::DEBUG;
            break;  // LOG_DEBUG
        case 3:
            level = LogLevel::INFO;
            break;  // LOG_INFO
        case 4:
            level = LogLevel::WARNING;
            break;  // LOG_WARNING
        case 5:
            level = LogLevel::Error;
            break;  // LOG_ERROR
        default:
            level = LogLevel::INFO;
            break;
    }

    LogEntry entry(level, topic, cleanMessage);

    WriteLogToStdout(entry);

    {
        std::lock_guard<std::mutex> lock(logMutex_);
        pendingLogs_.push(entry);
        discoveredTopics_.insert(topic);
    }
}

void LogService::LogMessage(LogLevel level, const std::string& topic, const std::string& message) {
    LogEntry entry(level, topic, message);

    WriteLogToStdout(entry);

    {
        std::lock_guard<std::mutex> lock(logMutex_);
        pendingLogs_.push(entry);
        discoveredTopics_.insert(topic);
    }
}

void LogService::WriterThreadFunction() {
    std::queue<LogEntry> localQueue;

    while (!shouldStop_) {
        {
            std::lock_guard<std::mutex> lock(logMutex_);
            if (!pendingLogs_.empty()) {
                localQueue.swap(pendingLogs_);
            }
        }

        while (!localQueue.empty()) {
            WriteLogToFile(localQueue.front());
            localQueue.pop();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    {
        std::lock_guard<std::mutex> lock(logMutex_);
        localQueue.swap(pendingLogs_);
    }

    while (!localQueue.empty()) {
        WriteLogToFile(localQueue.front());
        localQueue.pop();
    }
}

void LogService::WriteLogToFile(const LogEntry& entry) {
    if (!logFile_.is_open())
        return;

    std::string timeStr = FormatTimestamp(entry.timestamp);
    std::string levelStr = GetLogLevelName(entry.level);

    logFile_ << "[" << timeStr << "] [" << levelStr << "] [" << entry.topic << "] " << entry.message << std::endl;
    logFile_.flush();
}

void LogService::WriteLogToStdout(const LogEntry& entry) {
    const char* levelColor = GetLogLevelColor(entry.level);
    const char* levelName = GetLogLevelName(entry.level);
    const char* resetColor = "\033[0m";

    printf("%s[%s] [%s] %s%s\n", levelColor, levelName, entry.topic.c_str(), entry.message.c_str(), resetColor);
}

std::string LogService::FormatTimestamp(const std::chrono::system_clock::time_point& timestamp) const {
    auto time_t = std::chrono::system_clock::to_time_t(timestamp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  timestamp.time_since_epoch()) %
              1000;

    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();

    return ss.str();
}

// Static standardized logging methods
void LogService::LogInfo(const std::string& topic, const std::string& message) {
    TraceLog(LOG_INFO, "[%s] - %s", topic.c_str(), message.c_str());
}

void LogService::LogWarning(const std::string& topic, const std::string& message) {
    TraceLog(LOG_WARNING, "[%s] - %s", topic.c_str(), message.c_str());
}

void LogService::LogError(const std::string& topic, const std::string& message) {
    TraceLog(LOG_ERROR, "[%s] - %s", topic.c_str(), message.c_str());
}

void LogService::LogDebug(const std::string& topic, const std::string& message) {
    TraceLog(LOG_DEBUG, "[%s] - %s", topic.c_str(), message.c_str());
}

// Enhanced LogLevel-based methods
const char* LogService::GetLogLevelName(LogLevel level) const {
    switch (level) {
        case LogLevel::DEBUG:
            return "DEBUG";
        case LogLevel::INFO:
            return "INFO";
        case LogLevel::WARNING:
            return "WARNING";
        case LogLevel::Error:
            return "ERROR";
        default:
            return "UNKNOWN";
    }
}

const char* LogService::GetLogLevelColor(LogLevel level) const {
    switch (level) {
        case LogLevel::DEBUG:
            return "\033[37m";  // Light gray
        case LogLevel::INFO:
            return "\033[37m";  // White
        case LogLevel::WARNING:
            return "\033[93m";  // Yellow
        case LogLevel::Error:
            return "\033[91m";  // Red
        default:
            return "\033[37m";
    }
}

std::vector<std::string> LogService::GetAllTopics() const {
    std::vector<std::string> topics;
    for (const auto& topic : discoveredTopics_) {
        topics.push_back(topic);
    }
    std::sort(topics.begin(), topics.end());
    return topics;
}

std::string LogService::FormatLogEntry(const LogEntry& entry) const {
    std::string timeStr = FormatTimestamp(entry.timestamp);
    std::string levelStr = GetLogLevelName(entry.level);
    return "[" + timeStr + "] [" + levelStr + "] [" + entry.topic + "] " + entry.message;
}

}  // namespace Elysium::Services
