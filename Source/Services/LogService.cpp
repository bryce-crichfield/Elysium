#include "Services/LogService.h"
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include "Core/Common.h"
#include "raylib.h"

namespace Elysium::Services {

namespace {

// raylib's TraceLog callback has no user-data slot, so it needs a static
// pointer back to the one LogService instance. Hooked in the constructor
// (not Initialize()) so it's live before ApplicationConfig::FromXML runs.
LogService* g_logServiceInstance = nullptr;

void TraceLogCallback(int logLevel, const char* text, va_list args) {
    if (!g_logServiceInstance) return;
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), text, args);
    g_logServiceInstance->LogMessage(logLevel, std::string(buffer));
}

}  // namespace

LogService::LogService(ServiceLocator&)
    : initialized_(false), shouldStop_(false) {
    logBuffer_.reserve(MAX_LOG_BUFFER_SIZE);

    g_logServiceInstance = this;
    SetTraceLogCallback(TraceLogCallback);
    SetTraceLogLevel(LOG_DEBUG);
}

LogService::~LogService() {
    Shutdown();
    if (g_logServiceInstance == this) {
        g_logServiceInstance = nullptr;
    }
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
    writerOwnsStdout_ = true;

    initialized_ = true;
}

void LogService::Shutdown() {
    if (!initialized_)
        return;

    shouldStop_ = true;
    if (writerThread_.joinable()) {
        writerThread_.join();
    }
    writerOwnsStdout_ = false;  // nothing drains any more; later logs print inline

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

    if (!writerOwnsStdout_) WriteLogToStdoutNow(entry);

    {
        std::lock_guard<std::mutex> lock(logMutex_);
        pendingLogs_.push(entry);
        discoveredTopics_.insert(topic);
    }
}

void LogService::LogMessage(LogLevel level, const std::string& topic, const std::string& message) {
    LogEntry entry(level, topic, message);

    if (!writerOwnsStdout_) WriteLogToStdoutNow(entry);

    {
        std::lock_guard<std::mutex> lock(logMutex_);
        pendingLogs_.push(entry);
        discoveredTopics_.insert(topic);
    }
}

void LogService::WriterThreadFunction() {
    ProfileThread("Log Writer");
    std::queue<LogEntry> localQueue;

    while (!shouldStop_) {
        {
            std::lock_guard<std::mutex> lock(logMutex_);
            if (!pendingLogs_.empty()) {
                localQueue.swap(pendingLogs_);
            }
        }

        DrainToSinks(localQueue);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    {
        std::lock_guard<std::mutex> lock(logMutex_);
        localQueue.swap(pendingLogs_);
    }
    DrainToSinks(localQueue);
}

// One file write and one console write per batch, rather than one of each per line.
void LogService::DrainToSinks(std::queue<LogEntry>& batch) {
    if (batch.empty()) return;

    std::string console;
    while (!batch.empty()) {
        const LogEntry& entry = batch.front();
        WriteLogToFile(entry);
        AppendStdoutLine(entry, console);
        batch.pop();
    }
    if (logFile_.is_open()) logFile_.flush();
    if (!console.empty()) {
        fwrite(console.data(), 1, console.size(), stdout);
        fflush(stdout);
    }
}

void LogService::WriteLogToFile(const LogEntry& entry) {
    if (!logFile_.is_open())
        return;

    std::string timeStr = FormatTimestamp(entry.timestamp);
    std::string levelStr = GetLogLevelName(entry.level);

    // A bare newline rather than std::endl, and no flush here: std::endl flushes, and the
    // explicit flush() after it made two syscalls per line. WriterThreadFunction flushes once
    // per drain instead, so at most 100ms of log is unflushed if the process dies.
    logFile_ << "[" << timeStr << "] [" << levelStr << "] [" << entry.topic << "] " << entry.message << '\n';
}

void LogService::AppendStdoutLine(const LogEntry& entry, std::string& out) const {
    out += GetLogLevelColor(entry.level);
    out += '[';
    out += GetLogLevelName(entry.level);
    out += "] [";
    out += entry.topic;
    out += "] ";
    out += entry.message;
    out += "\033[0m";
    out += '\n';
}

void LogService::WriteLogToStdoutNow(const LogEntry& entry) const {
    std::string line;
    AppendStdoutLine(entry, line);
    fwrite(line.data(), 1, line.size(), stdout);
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
