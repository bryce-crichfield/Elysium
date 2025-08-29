#include "Services/LogService.h"
#include "imgui.h"
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include "Common.h"

namespace Elysium::Services {

LogService::LogService()
    : initialized_(false), shouldStop_(false) {
    name_ = "LogService";
    logBuffer_.reserve(MAX_LOG_BUFFER_SIZE);
    InitializeFilters();
}

LogService::~LogService() {
    Shutdown();
}

void LogService::Initialize() {
    Initialize("logs/engine.log");
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

void LogService::OnDebugDraw() {
    Profile;
    DrawHeader();
    ImGui::Separator();

    if (showFilterPanel_) {
        DrawFilterPanel();
    }

    DrawLogEntries();
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
        case 2: level = LogLevel::DEBUG; break;   // LOG_DEBUG
        case 3: level = LogLevel::INFO; break;    // LOG_INFO
        case 4: level = LogLevel::WARNING; break; // LOG_WARNING
        case 5: level = LogLevel::ERROR; break;   // LOG_ERROR
        default: level = LogLevel::INFO; break;
    }

    LogEntry entry(level, topic, cleanMessage);

    WriteLogToStdout(entry);

    {
        std::lock_guard<std::mutex> lock(logMutex_);
        pendingLogs_.push(entry);
        discoveredTopics_.insert(topic);

        // Auto-enable new topics in filter
        if (topicFilters_.find(topic) == topicFilters_.end()) {
            topicFilters_[topic] = true;
        }
    }
}

void LogService::LogMessage(LogLevel level, const std::string& topic, const std::string& message) {
    LogEntry entry(level, topic, message);

    WriteLogToStdout(entry);

    {
        std::lock_guard<std::mutex> lock(logMutex_);
        pendingLogs_.push(entry);
        discoveredTopics_.insert(topic);

        // Auto-enable new topics in filter
        if (topicFilters_.find(topic) == topicFilters_.end()) {
            topicFilters_[topic] = true;
        }
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
        timestamp.time_since_epoch()) % 1000;

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
        case LogLevel::DEBUG:   return "DEBUG";
        case LogLevel::INFO:    return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR:   return "ERROR";
        default:                return "UNKNOWN";
    }
}

const char* LogService::GetLogLevelColor(LogLevel level) const {
    switch (level) {
        case LogLevel::DEBUG:   return "\033[37m";  // Light gray
        case LogLevel::INFO:    return "\033[37m";  // White
        case LogLevel::WARNING: return "\033[93m";  // Yellow
        case LogLevel::ERROR:   return "\033[91m";  // Red
        default:                return "\033[37m";
    }
}

unsigned int LogService::GetImGuiColor(LogLevel level) const {
    switch (level) {
        case LogLevel::DEBUG:   return IM_COL32(173, 216, 230, 255); // Light blue
        case LogLevel::INFO:    return IM_COL32(255, 255, 255, 255); // White
        case LogLevel::WARNING: return IM_COL32(255, 255, 0, 255);   // Yellow
        case LogLevel::ERROR:   return IM_COL32(255, 100, 100, 255); // Red
        default:                return IM_COL32(255, 255, 255, 255);
    }
}

bool LogService::ShouldDisplayEntry(const LogEntry& entry) const {
    // Check level filter
    auto levelIt = levelFilters_.find(entry.level);
    if (levelIt != levelFilters_.end() && !levelIt->second) {
        return false;
    }

    // Check topic filter
    auto topicIt = topicFilters_.find(entry.topic);
    if (topicIt != topicFilters_.end() && !topicIt->second) {
        return false;
    }

    // Check search filter (case-insensitive)
    if (!searchFilter_.empty()) {
        std::string lowerSearch = searchFilter_;
        std::transform(lowerSearch.begin(), lowerSearch.end(), lowerSearch.begin(), ::tolower);

        std::string lowerMessage = entry.message;
        std::transform(lowerMessage.begin(), lowerMessage.end(), lowerMessage.begin(), ::tolower);

        std::string lowerTopic = entry.topic;
        std::transform(lowerTopic.begin(), lowerTopic.end(), lowerTopic.begin(), ::tolower);

        if (lowerMessage.find(lowerSearch) == std::string::npos &&
            lowerTopic.find(lowerSearch) == std::string::npos) {
            return false;
        }
    }

    return true;
}

void LogService::InitializeFilters() {
    // Enable all log levels by default
    levelFilters_[LogLevel::DEBUG] = true;
    levelFilters_[LogLevel::INFO] = true;
    levelFilters_[LogLevel::WARNING] = true;
    levelFilters_[LogLevel::ERROR] = true;
}

std::vector<std::string> LogService::GetAllTopics() const {
    std::vector<std::string> topics;
    for (const auto& topic : discoveredTopics_) {
        topics.push_back(topic);
    }
    std::sort(topics.begin(), topics.end());
    return topics;
}

void LogService::DrawHeader() {
    ImGui::Text("Log Entries: %zu", logBuffer_.size());
    ImGui::SameLine();
    ImGui::Text("Topics: %zu", discoveredTopics_.size());
    ImGui::SameLine();

    if (ImGui::Button(showFilterPanel_ ? "Hide Filters" : "Show Filters")) {
        showFilterPanel_ = !showFilterPanel_;
    }
    ImGui::SameLine();

    // Search bar
    ImGui::Text("Search:");
    ImGui::SameLine();
    static char searchBuffer[256] = "";
    ImGui::PushItemWidth(200);
    if (ImGui::InputText("##LogSearch", searchBuffer, sizeof(searchBuffer))) {
        searchFilter_ = std::string(searchBuffer);
    }
    ImGui::PopItemWidth();
    ImGui::SameLine();

    if (ImGui::SmallButton("Clear##Search")) {
        searchBuffer[0] = '\0';
        searchFilter_.clear();
    }
    ImGui::SameLine();

    // Copy selected button
    if (ImGui::Button("Copy Selected")) {
        if (!selectedLogIndices_.empty()) {
            std::string combinedText = "";
            std::lock_guard<std::mutex> lock(logMutex_);
            for (int idx : selectedLogIndices_) {
                if (idx >= 0 && idx < (int)logBuffer_.size()) {
                    const LogEntry& entry = logBuffer_[idx];
                    combinedText += FormatLogEntry(entry) + "\n";
                }
            }
            if (!combinedText.empty()) {
                ImGui::SetClipboardText(combinedText.c_str());
            }
        }
    }

    if (!selectedLogIndices_.empty()) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "(%zu selected)", selectedLogIndices_.size());
    }
}

void LogService::DrawFilterPanel() {
    if (ImGui::BeginChild("FilterPanel", ImVec2(0, 200), true)) {
        DrawLevelFilters();
        ImGui::SameLine();
        DrawTopicFilters();
    }
    ImGui::EndChild();
}

void LogService::DrawLevelFilters() {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.1f, 0.1f, 0.1f, 0.3f));
    if (ImGui::BeginChild("LevelFilters", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f - 5, 180), true)) {
        ImGui::Text("Log Levels");
        ImGui::Separator();

        // Color-coded level checkboxes
        struct LevelFilter { LogLevel level; const char* name; ImU32 color; };
        const LevelFilter levels[] = {
            {LogLevel::DEBUG, "DEBUG", IM_COL32(173, 216, 230, 255)},
            {LogLevel::INFO, "INFO", IM_COL32(255, 255, 255, 255)},
            {LogLevel::WARNING, "WARNING", IM_COL32(255, 255, 0, 255)},
            {LogLevel::ERROR, "ERROR", IM_COL32(255, 100, 100, 255)}
        };

        for (const auto& levelFilter : levels) {
            ImGui::PushStyleColor(ImGuiCol_Text, levelFilter.color);
            ImGui::Checkbox(levelFilter.name, &levelFilters_[levelFilter.level]);
            ImGui::PopStyleColor();
            ImGui::SameLine();
        }
        ImGui::NewLine();

        // Quick toggle buttons
        if (ImGui::SmallButton("All Levels")) {
            for (auto& pair : levelFilters_) pair.second = true;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Clear Levels")) {
            for (auto& pair : levelFilters_) pair.second = false;
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void LogService::DrawTopicFilters() {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.1f, 0.1f, 0.1f, 0.3f));
    if (ImGui::BeginChild("TopicFilters", ImVec2(0, 180), true)) {
        ImGui::Text("Topics (%zu)", discoveredTopics_.size());
        ImGui::SameLine();

        if (ImGui::SmallButton("All Topics")) {
            for (auto& pair : topicFilters_) pair.second = true;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Clear Topics")) {
            for (auto& pair : topicFilters_) pair.second = false;
        }
        ImGui::Separator();

        // Topic checkboxes in organized rows
        auto topics = GetAllTopics();
        int itemsPerRow = std::max(1, (int)(ImGui::GetContentRegionAvail().x / 120));

        for (size_t i = 0; i < topics.size(); ++i) {
            const std::string& topic = topics[i];
            bool& enabled = topicFilters_[topic];

            ImGui::Checkbox(topic.c_str(), &enabled);

            if ((i + 1) % itemsPerRow != 0 && i + 1 < topics.size()) {
                ImGui::SameLine();
            }
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void LogService::DrawLogEntries() {
    if (ImGui::BeginChild("LogScrollRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar)) {
        std::lock_guard<std::mutex> lock(logMutex_);

        // Collect visible log indices
        size_t startIdx = logBuffer_.size() > MAX_DISPLAY_LOGS ?
                         logBuffer_.size() - MAX_DISPLAY_LOGS : 0;

        std::vector<int> visibleIndices;
        for (size_t i = startIdx; i < logBuffer_.size(); ++i) {
            if (ShouldDisplayEntry(logBuffer_[i])) {
                visibleIndices.push_back((int)i);
            }
        }

        // Render each visible log entry
        for (int logIndex : visibleIndices) {
            const LogEntry& entry = logBuffer_[logIndex];
            std::string fullLogText = FormatLogEntry(entry);

            ImGui::PushID(logIndex);

            bool isSelected = selectedLogIndices_.find(logIndex) != selectedLogIndices_.end();
            if (isSelected) {
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
                ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(100, 100, 150, 128));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(120, 120, 170, 128));
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, IM_COL32(140, 140, 190, 128));
            }

            if (ImGui::Selectable(("##log_" + std::to_string(logIndex)).c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick)) {
                HandleLogSelection(logIndex, visibleIndices);

                if (ImGui::IsMouseDoubleClicked(0)) {
                    ImGui::SetClipboardText(fullLogText.c_str());
                }
            }

            if (isSelected) {
                ImGui::PopStyleColor(4);
            }

            // Draw log text with appropriate color
            ImGui::SameLine(0, 0);
            ImGui::PushStyleColor(ImGuiCol_Text, GetImGuiColor(entry.level));
            ImGui::TextUnformatted(fullLogText.c_str());
            ImGui::PopStyleColor();

            DrawLogContextMenu(logIndex, entry, fullLogText);
            ImGui::PopID();
        }

        // Show no results message if needed
        if (visibleIndices.empty() && logBuffer_.size() > 0) {
            ImGui::TextColored(ImVec4(1, 1, 0, 1), "No entries match current filters. Total entries: %zu", logBuffer_.size());
        }

        // Auto-scroll to bottom
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
            ImGui::SetScrollHereY(1.0f);
        }
    }
    ImGui::EndChild();
}

void LogService::HandleLogSelection(int logIndex, const std::vector<int>& visibleIndices) {
    bool ctrlPressed = ImGui::GetIO().KeyCtrl;
    bool shiftPressed = ImGui::GetIO().KeyShift;
    bool isSelected = selectedLogIndices_.find(logIndex) != selectedLogIndices_.end();

    if (shiftPressed && dragStartIndex_ != -1) {
        // Range selection
        selectedLogIndices_.clear();
        int start = std::min(dragStartIndex_, logIndex);
        int end = std::max(dragStartIndex_, logIndex);
        for (int idx : visibleIndices) {
            if (idx >= start && idx <= end) {
                selectedLogIndices_.insert(idx);
            }
        }
    } else if (ctrlPressed) {
        // Toggle selection
        if (isSelected) {
            selectedLogIndices_.erase(logIndex);
        } else {
            selectedLogIndices_.insert(logIndex);
            dragStartIndex_ = logIndex;
        }
    } else {
        // Single selection
        selectedLogIndices_.clear();
        selectedLogIndices_.insert(logIndex);
        dragStartIndex_ = logIndex;
    }
}

void LogService::DrawLogContextMenu(int logIndex, const LogEntry& entry, const std::string& fullLogText) {
    if (ImGui::BeginPopupContextItem(("log_context_" + std::to_string(logIndex)).c_str())) {
        if (ImGui::MenuItem("Copy This Line")) {
            ImGui::SetClipboardText(fullLogText.c_str());
        }
        if (ImGui::MenuItem("Copy Message Only")) {
            ImGui::SetClipboardText(entry.message.c_str());
        }
        if (ImGui::MenuItem("Copy Topic")) {
            ImGui::SetClipboardText(entry.topic.c_str());
        }
        if (ImGui::MenuItem("Copy Timestamp")) {
            std::string timeStr = FormatTimestamp(entry.timestamp);
            ImGui::SetClipboardText(timeStr.c_str());
        }

        if (selectedLogIndices_.size() > 1) {
            ImGui::Separator();
            if (ImGui::MenuItem("Copy All Selected")) {
                std::string combinedText = "";
                std::lock_guard<std::mutex> lock(logMutex_);
                for (int idx : selectedLogIndices_) {
                    if (idx >= 0 && idx < (int)logBuffer_.size()) {
                        combinedText += FormatLogEntry(logBuffer_[idx]) + "\n";
                    }
                }
                if (!combinedText.empty()) {
                    ImGui::SetClipboardText(combinedText.c_str());
                }
            }
        }
        ImGui::EndPopup();
    }
}

std::string LogService::FormatLogEntry(const LogEntry& entry) const {
    std::string timeStr = FormatTimestamp(entry.timestamp);
    std::string levelStr = GetLogLevelName(entry.level);
    return "[" + timeStr + "] [" + levelStr + "] [" + entry.topic + "] " + entry.message;
}

} // namespace Elysium::Services
