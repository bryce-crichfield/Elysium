#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "Core/Editor.h"

namespace Elysium::Services {
class ILogService;
enum class LogLevel;
struct LogEntry;
}  // namespace Elysium::Services

namespace Elysium {

class LogEditor : public Editor {
   public:
    explicit LogEditor(ServiceLocator& services);

    void Draw() override;

   private:
    void DrawHeader(Services::ILogService& service);
    void DrawFilterPanel(Services::ILogService& service);
    void DrawLevelFilters();
    void DrawTopicFilters(Services::ILogService& service);
    void DrawLogEntries(Services::ILogService& service);
    void HandleLogSelection(int logIndex, const std::vector<int>& visibleIndices);
    void DrawLogContextMenu(Services::ILogService& service, int logIndex, const Services::LogEntry& entry,
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
