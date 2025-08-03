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