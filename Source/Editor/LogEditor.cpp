#include "LogEditor.h"
#include <algorithm>
#include "Core/Common.h"
#include "Editor/Widgets.h"
#include "Interfaces/ILogService.h"

namespace Elysium {

using namespace Services;

namespace {
struct LevelInfo {
    LogLevel level;
    const char* name;
};
constexpr LevelInfo kLevels[] = {
    {LogLevel::DEBUG, "Debug"},
    {LogLevel::INFO, "Info"},
    {LogLevel::WARNING, "Warning"},
    {LogLevel::Error, "Error"},
};
}  // namespace

LogEditor::LogEditor(ServiceLocator& services) : Editor(services, Title) {
    for (const auto& info : kLevels) levelFilters_[info.level] = true;
}

void LogEditor::Draw() {
    Profile;

    auto& service = services_.Get<ILogService>();

    if (BeginWindow()) {
        DrawToolbar(service);
        ImGui::Separator();
        DrawLogEntries(service);
    }
    EndWindow();
}

void LogEditor::DrawToolbar(ILogService& service) {
    SearchField("##LogSearch", searchBuffer_, sizeof(searchBuffer_), Theme::SearchWidth);

    DrawLevelToggles();

    ImGui::SameLine();
    DrawTopicFilter(service);

    // Right side: entry count and copy.
    const std::string count = selectedLogIndices_.empty()
        ? std::to_string(service.GetLogBuffer().size()) + " entries"
        : std::to_string(selectedLogIndices_.size()) + " selected";
    AlignRight(ImGui::CalcTextSize(count.c_str()).x + ButtonWidth(ICON_FA_COPY) + ImGui::GetStyle().ItemSpacing.x);
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("%s", count.c_str());
    ImGui::SameLine();
    ImGui::BeginDisabled(selectedLogIndices_.empty());
    if (IconButton(ICON_FA_COPY, "Copy selected entries")) CopySelection(service);
    ImGui::EndDisabled();
}

// One toggle per level, tinted with its log color and dimmed while hidden.
void LogEditor::DrawLevelToggles() {
    for (const auto& info : kLevels) {
        bool& enabled = levelFilters_[info.level];
        ImGui::PushStyleColor(ImGuiCol_Text, enabled ? LevelColor(info.level) : Palette::TextDisabled);
        ImGui::PushStyleColor(ImGuiCol_Button, enabled ? Palette::Surface0 : Palette::WithAlpha(Palette::Surface0, 0.0f));
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        if (ImGui::Button(info.name)) enabled = !enabled;
        ImGui::PopStyleColor(2);
        ItemTooltip(enabled ? "Hide this level" : "Show this level");
    }
}

void LogEditor::DrawTopicFilter(ILogService& service) {
    const auto topics = service.GetAllTopics();
    // Auto-enable new topics
    for (const auto& topic : topics) topicFilters_.try_emplace(topic, true);

    const bool anyHidden = std::any_of(topicFilters_.begin(), topicFilters_.end(), [](const auto& p) { return !p.second; });
    if (anyHidden) ImGui::PushStyleColor(ImGuiCol_Text, Palette::Accent);
    if (ImGui::Button(ICON_FA_FILTER "  Topics")) ImGui::OpenPopup("TopicFilter");
    if (anyHidden) ImGui::PopStyleColor();

    if (!ImGui::BeginPopup("TopicFilter")) return;
    if (ImGui::SmallButton("All")) for (auto& [topic, enabled] : topicFilters_) enabled = true;
    ImGui::SameLine();
    if (ImGui::SmallButton("None")) for (auto& [topic, enabled] : topicFilters_) enabled = false;
    ImGui::Separator();
    for (const auto& topic : topics) ImGui::Checkbox(topic.c_str(), &topicFilters_[topic]);
    ImGui::EndPopup();
}

void LogEditor::DrawLogEntries(ILogService& service) {
    if (ImGui::BeginChild("LogScrollRegion", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar)) {
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
            if (ImGui::Selectable("##log", isSelected, ImGuiSelectableFlags_AllowDoubleClick)) {
                HandleLogSelection(logIndex, visibleIndices);

                if (ImGui::IsMouseDoubleClicked(0)) {
                    ImGui::SetClipboardText(fullLogText.c_str());
                }
            }

            ImGui::SameLine(0, 0);
            ColoredText(LevelColor(entry.level), fullLogText.c_str());

            DrawLogContextMenu(service, logIndex, entry, fullLogText);
            ImGui::PopID();
        }

        if (visibleIndices.empty()) {
            EmptyState(logBuffer.empty() ? "No log entries yet" : "No entries match the current filters");
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

void LogEditor::CopySelection(ILogService& service) const {
    const auto& logBuffer = service.GetLogBuffer();
    std::vector<int> indices(selectedLogIndices_.begin(), selectedLogIndices_.end());
    std::sort(indices.begin(), indices.end());

    std::string combinedText;
    for (int idx : indices) {
        if (idx >= 0 && idx < (int)logBuffer.size()) combinedText += service.FormatLogEntry(logBuffer[idx]) + "\n";
    }
    if (!combinedText.empty()) ImGui::SetClipboardText(combinedText.c_str());
}

void LogEditor::DrawLogContextMenu(ILogService& service, int logIndex, const LogEntry& entry,
                                   const std::string& fullLogText) {
    if (!ImGui::BeginPopupContextItem("LogContext")) return;

    if (ImGui::MenuItem("Copy Line")) ImGui::SetClipboardText(fullLogText.c_str());
    if (ImGui::MenuItem("Copy Message")) ImGui::SetClipboardText(entry.message.c_str());
    if (ImGui::MenuItem("Copy Topic")) ImGui::SetClipboardText(entry.topic.c_str());
    if (ImGui::MenuItem("Copy Timestamp")) ImGui::SetClipboardText(service.FormatTimestamp(entry.timestamp).c_str());

    if (selectedLogIndices_.size() > 1) {
        ImGui::Separator();
        if (ImGui::MenuItem("Copy All Selected")) CopySelection(service);
    }
    ImGui::EndPopup();
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

    return MatchesSearch(entry.message, searchBuffer_) || MatchesSearch(entry.topic, searchBuffer_);
}

ImVec4 LogEditor::LevelColor(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG:   return Palette::Debug;
        case LogLevel::WARNING: return Palette::Warning;
        case LogLevel::Error:   return Palette::Error;
        default:                return Palette::Text;
    }
}

}  // namespace Elysium
