#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "Core/Editor.h"

namespace Elysium::Services {
class LogService;
enum class LogLevel;
struct LogEntry;
}  // namespace Elysium::Services

namespace Elysium {

class LogEditor : public Editor {
   public:
    LogEditor();

    void Draw(Application& app) override;

   private:
    void DrawHeader(Services::LogService& service);
    void DrawFilterPanel(Services::LogService& service);
    void DrawLevelFilters();
    void DrawTopicFilters(Services::LogService& service);
    void DrawLogEntries(Services::LogService& service);
    void HandleLogSelection(int logIndex, const std::vector<int>& visibleIndices);
    void DrawLogContextMenu(Services::LogService& service, int logIndex, const Services::LogEntry& entry,
                            const std::string& fullLogText);

    bool ShouldDisplayEntry(const Services::LogEntry& entry) const;
    unsigned int GetImGuiColor(Services::LogLevel level) const;

    // Filter state
    std::unordered_map<std::string, bool> topicFilters_;
    std::unordered_map<Services::LogLevel, bool> levelFilters_;
    bool showFilterPanel_ = false;
    std::string searchFilter_;

    // Selection state
    std::unordered_set<int> selectedLogIndices_;
    int dragStartIndex_ = -1;

    // Display constants
    static constexpr size_t MAX_DISPLAY_LOGS = 500;
};

}  // namespace Elysium
