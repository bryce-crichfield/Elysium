#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "Editor/Editor.h"

namespace Elysium::Services {
class ILogService;
enum class LogLevel;
struct LogEntry;
}  // namespace Elysium::Services

namespace Elysium {

// The log console: a toolbar (search, level toggles, topic filter, copy) over the
// scrolling, selectable log.
class LogEditor : public Editor {
   public:
    static constexpr const char* Title = "Console";

    explicit LogEditor(ServiceLocator& services);

    void Draw() override;

   private:
    void DrawToolbar(Services::ILogService& service);
    void DrawLevelToggles();
    void DrawTopicFilter(Services::ILogService& service);
    void DrawLogEntries(Services::ILogService& service);
    void HandleLogSelection(int logIndex, const std::vector<int>& visibleIndices);
    void DrawLogContextMenu(Services::ILogService& service, int logIndex, const Services::LogEntry& entry,
                            const std::string& fullLogText);
    void CopySelection(Services::ILogService& service) const;

    bool ShouldDisplayEntry(const Services::LogEntry& entry) const;
    static ImVec4 LevelColor(Services::LogLevel level);

    // Filter state
    std::unordered_map<std::string, bool> topicFilters_;
    std::unordered_map<Services::LogLevel, bool> levelFilters_;
    char searchBuffer_[256] = "";

    // Selection state
    std::unordered_set<int> selectedLogIndices_;
    int dragStartIndex_ = -1;

    // Display constants
    static constexpr size_t MAX_DISPLAY_LOGS = 500;
};

}  // namespace Elysium
