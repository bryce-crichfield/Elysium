#include "Services/LogService.h"
#include "imgui.h"
#include <cstdio>
#include <cstdarg>
#include <filesystem>
#include <iomanip>
#include <sstream>

namespace Elysium::Services {

LogService::LogService() 
    : isVisible_(false), initialized_(false), shouldStop_(false) {
    logBuffer_.reserve(MAX_LOG_BUFFER_SIZE);
}

LogService::~LogService() {
    Shutdown();
}

void LogService::Initialize(const std::string& logFilePath) {
    if (initialized_) return;
    
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
    if (!initialized_) return;
    
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
    std::lock_guard<std::mutex> lock(logMutex_);
    
    while (!pendingLogs_.empty()) {
        logBuffer_.push_back(pendingLogs_.front());
        pendingLogs_.pop();
        
        if (logBuffer_.size() > MAX_LOG_BUFFER_SIZE) {
            logBuffer_.erase(logBuffer_.begin());
        }
    }
}

void LogService::Draw() {
    if (!isVisible_) return;
    
    ImGui::SetNextWindowPos(ImVec2(10, 300), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(800, 400), ImGuiCond_FirstUseEver);
    
    if (ImGui::Begin("Log Console", &isVisible_, ImGuiWindowFlags_NoCollapse)) {
        ImGui::Text("Log Entries: %zu", logBuffer_.size());
        ImGui::Separator();
        
        if (ImGui::BeginChild("LogScrollRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar)) {
            std::lock_guard<std::mutex> lock(logMutex_);
            
            size_t startIdx = logBuffer_.size() > MAX_DISPLAY_LOGS ? 
                             logBuffer_.size() - MAX_DISPLAY_LOGS : 0;
            
            for (size_t i = startIdx; i < logBuffer_.size(); ++i) {
                const LogEntry& entry = logBuffer_[i];
                
                std::string timeStr = FormatTimestamp(entry.timestamp);
                std::string levelStr = GetLogLevelName(entry.level);
                
                ImGui::PushStyleColor(ImGuiCol_Text, GetImGuiColor(entry.level));
                ImGui::TextUnformatted(("[" + timeStr + "] [" + levelStr + "] " + entry.message).c_str());
                ImGui::PopStyleColor();
            }
            
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                ImGui::SetScrollHereY(1.0f);
            }
        }
        ImGui::EndChild();
    }
    ImGui::End();
}

void LogService::LogMessage(int logLevel, const std::string& message) {
    LogEntry entry(logLevel, message);
    
    WriteLogToStdout(entry);
    
    {
        std::lock_guard<std::mutex> lock(logMutex_);
        pendingLogs_.push(entry);
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
    if (!logFile_.is_open()) return;
    
    std::string timeStr = FormatTimestamp(entry.timestamp);
    std::string levelStr = GetLogLevelName(entry.level);
    
    logFile_ << "[" << timeStr << "] [" << levelStr << "] " << entry.message << std::endl;
    logFile_.flush();
}

void LogService::WriteLogToStdout(const LogEntry& entry) {
    const char* levelColor = GetLogLevelColor(entry.level);
    const char* levelName = GetLogLevelName(entry.level);
    const char* resetColor = "\033[0m";
    
    printf("%s[%s] %s%s\n", levelColor, levelName, entry.message.c_str(), resetColor);
}

const char* LogService::GetLogLevelName(int level) const {
    static const char* levelNames[] = {
        "ALL", "TRACE", "DEBUG", "INFO", "WARNING", "ERROR", "FATAL", "NONE"
    };
    int safeLevel = (level >= 0 && level < 8) ? level : 3;
    return levelNames[safeLevel];
}

const char* LogService::GetLogLevelColor(int level) const {
    static const char* levelColors[] = {
        "\033[37m", "\033[37m", "\033[37m", "\033[37m",
        "\033[93m", "\033[91m", "\033[95m", "\033[90m"
    };
    int safeLevel = (level >= 0 && level < 8) ? level : 3;
    return levelColors[safeLevel];
}

unsigned int LogService::GetImGuiColor(int level) const {
    switch (level) {
        case LOG_TRACE:   return IM_COL32(128, 128, 128, 255);
        case LOG_DEBUG:   return IM_COL32(173, 216, 230, 255);
        case LOG_INFO:    return IM_COL32(255, 255, 255, 255);
        case LOG_WARNING: return IM_COL32(255, 255, 0, 255);
        case LOG_ERROR:   return IM_COL32(255, 100, 100, 255);
        case LOG_FATAL:   return IM_COL32(255, 0, 255, 255);
        default:          return IM_COL32(255, 255, 255, 255);
    }
}

std::string LogService::FormatTimestamp(const std::chrono::system_clock::time_point& timestamp) const {
    auto time_t = std::chrono::system_clock::to_time_t(timestamp);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        timestamp.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    
    return ss.str();
}

} // namespace Elysium::Services