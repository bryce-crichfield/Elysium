#include "LogEditor.h"
#include <algorithm>
#include "Core/Application.h"
#include "Core/Common.h"
#include "Services/LogService.h"
#include "imgui.h"

namespace Elysium {

using namespace Services;

LogEditor::LogEditor() : Editor("Log Viewer") {
    // Enable all log levels by default
    levelFilters_[LogLevel::DEBUG] = true;
    levelFilters_[LogLevel::INFO] = true;
    levelFilters_[LogLevel::WARNING] = true;
    levelFilters_[LogLevel::Error] = true;
}

void LogEditor::Draw(Application& app) {
    Profile;

    auto& service = app.GetService<LogService>();

    ImGui::SetNextWindowSize(ImVec2(800, 500), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(name_.c_str(), nullptr, ImGuiWindowFlags_NoCollapse)) {
        DrawHeader(service);
        ImGui::Separator();

        if (showFilterPanel_) {
            DrawFilterPanel(service);
        }

        DrawLogEntries(service);
    }
    ImGui::End();
}

void LogEditor::DrawHeader(LogService& service) {
    const auto& logBuffer = service.GetLogBuffer();
    const auto& topics = service.GetAllTopics();

    ImGui::Text("Log Entries: %zu", logBuffer.size());
    ImGui::SameLine();
    ImGui::Text("Topics: %zu", topics.size());
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
            for (int idx : selectedLogIndices_) {
                if (idx >= 0 && idx < (int)logBuffer.size()) {
                    const LogEntry& entry = logBuffer[idx];
                    combinedText += service.FormatLogEntry(entry) + "\n";
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

void LogEditor::DrawFilterPanel(LogService& service) {
    if (ImGui::BeginChild("FilterPanel", ImVec2(0, 200), true)) {
        DrawLevelFilters();
        ImGui::SameLine();
        DrawTopicFilters(service);
    }
    ImGui::EndChild();
}

void LogEditor::DrawLevelFilters() {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.1f, 0.1f, 0.1f, 0.3f));
    if (ImGui::BeginChild("LevelFilters", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f - 5, 180), true)) {
        ImGui::Text("Log Levels");
        ImGui::Separator();

        struct LevelFilter {
            LogLevel level;
            const char* name;
            ImU32 color;
        };
        const LevelFilter levels[] = {
            {LogLevel::DEBUG, "DEBUG", IM_COL32(173, 216, 230, 255)},
            {LogLevel::INFO, "INFO", IM_COL32(255, 255, 255, 255)},
            {LogLevel::WARNING, "WARNING", IM_COL32(255, 255, 0, 255)},
            {LogLevel::Error, "ERROR", IM_COL32(255, 100, 100, 255)}};

        for (const auto& levelFilter : levels) {
            ImGui::PushStyleColor(ImGuiCol_Text, levelFilter.color);
            ImGui::Checkbox(levelFilter.name, &levelFilters_[levelFilter.level]);
            ImGui::PopStyleColor();
            ImGui::SameLine();
        }
        ImGui::NewLine();

        if (ImGui::SmallButton("All Levels")) {
            for (auto& pair : levelFilters_)
                pair.second = true;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Clear Levels")) {
            for (auto& pair : levelFilters_)
                pair.second = false;
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void LogEditor::DrawTopicFilters(LogService& service) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.1f, 0.1f, 0.1f, 0.3f));
    if (ImGui::BeginChild("TopicFilters", ImVec2(0, 180), true)) {
        auto topics = service.GetAllTopics();

        // Auto-enable new topics
        for (const auto& topic : topics) {
            if (topicFilters_.find(topic) == topicFilters_.end()) {
                topicFilters_[topic] = true;
            }
        }

        ImGui::Text("Topics (%zu)", topics.size());
        ImGui::SameLine();

        if (ImGui::SmallButton("All Topics")) {
            for (auto& pair : topicFilters_)
                pair.second = true;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Clear Topics")) {
            for (auto& pair : topicFilters_)
                pair.second = false;
        }
        ImGui::Separator();

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

void LogEditor::DrawLogEntries(LogService& service) {
    if (ImGui::BeginChild("LogScrollRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar)) {
        const auto& logBuffer = service.GetLogBuffer();

        size_t startIdx = logBuffer.size() > MAX_DISPLAY_LOGS ? logBuffer.size() - MAX_DISPLAY_LOGS : 0;

        std::vector<int> visibleIndices;
        for (size_t i = startIdx; i < logBuffer.size(); ++i) {
            if (ShouldDisplayEntry(logBuffer[i])) {
                visibleIndices.push_back((int)i);
            }
        }

        for (int logIndex : visibleIndices) {
            const LogEntry& entry = logBuffer[logIndex];
            std::string fullLogText = service.FormatLogEntry(entry);

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

            ImGui::SameLine(0, 0);
            ImGui::PushStyleColor(ImGuiCol_Text, GetImGuiColor(entry.level));
            ImGui::TextUnformatted(fullLogText.c_str());
            ImGui::PopStyleColor();

            DrawLogContextMenu(service, logIndex, entry, fullLogText);
            ImGui::PopID();
        }

        if (visibleIndices.empty() && logBuffer.size() > 0) {
            ImGui::TextColored(ImVec4(1, 1, 0, 1), "No entries match current filters. Total entries: %zu", logBuffer.size());
        }

        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
            ImGui::SetScrollHereY(1.0f);
        }
    }
    ImGui::EndChild();
}

void LogEditor::HandleLogSelection(int logIndex, const std::vector<int>& visibleIndices) {
    bool ctrlPressed = ImGui::GetIO().KeyCtrl;
    bool shiftPressed = ImGui::GetIO().KeyShift;
    bool isSelected = selectedLogIndices_.find(logIndex) != selectedLogIndices_.end();

    if (shiftPressed && dragStartIndex_ != -1) {
        selectedLogIndices_.clear();
        int start = std::min(dragStartIndex_, logIndex);
        int end = std::max(dragStartIndex_, logIndex);
        for (int idx : visibleIndices) {
            if (idx >= start && idx <= end) {
                selectedLogIndices_.insert(idx);
            }
        }
    } else if (ctrlPressed) {
        if (isSelected) {
            selectedLogIndices_.erase(logIndex);
        } else {
            selectedLogIndices_.insert(logIndex);
            dragStartIndex_ = logIndex;
        }
    } else {
        selectedLogIndices_.clear();
        selectedLogIndices_.insert(logIndex);
        dragStartIndex_ = logIndex;
    }
}

void LogEditor::DrawLogContextMenu(LogService& service, int logIndex, const LogEntry& entry,
                                   const std::string& fullLogText) {
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
            std::string timeStr = service.FormatTimestamp(entry.timestamp);
            ImGui::SetClipboardText(timeStr.c_str());
        }

        if (selectedLogIndices_.size() > 1) {
            ImGui::Separator();
            if (ImGui::MenuItem("Copy All Selected")) {
                std::string combinedText = "";
                const auto& logBuffer = service.GetLogBuffer();
                for (int idx : selectedLogIndices_) {
                    if (idx >= 0 && idx < (int)logBuffer.size()) {
                        combinedText += service.FormatLogEntry(logBuffer[idx]) + "\n";
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

bool LogEditor::ShouldDisplayEntry(const LogEntry& entry) const {
    auto levelIt = levelFilters_.find(entry.level);
    if (levelIt != levelFilters_.end() && !levelIt->second) {
        return false;
    }

    auto topicIt = topicFilters_.find(entry.topic);
    if (topicIt != topicFilters_.end() && !topicIt->second) {
        return false;
    }

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

unsigned int LogEditor::GetImGuiColor(LogLevel level) const {
    switch (level) {
        case LogLevel::DEBUG:
            return IM_COL32(173, 216, 230, 255);
        case LogLevel::INFO:
            return IM_COL32(255, 255, 255, 255);
        case LogLevel::WARNING:
            return IM_COL32(255, 255, 0, 255);
        case LogLevel::Error:
            return IM_COL32(255, 100, 100, 255);
        default:
            return IM_COL32(255, 255, 255, 255);
    }
}

}  // namespace Elysium
