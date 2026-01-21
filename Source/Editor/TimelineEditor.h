#pragma once

#include <string>
#include <vector>
#include "Core/Editor.h"
#include "Core/Entity.h"
#include "Core/Timeline.h"

namespace Elysium {
class Scene;
}

namespace Elysium::Services {
class TimelineService;
}

namespace Elysium {

class TimelineEditor : public Editor {
   public:
    TimelineEditor();

    void Draw(Application& app) override;

   private:
    Scene* currentScene_ = nullptr;
    Timeline* selectedTimeline_ = nullptr;
    TrackBase* selectedTrack_ = nullptr;

    enum class SelectionType { None,
                               Timeline,
                               Track,
                               CreateTimeline,
                               CreateTrack };
    SelectionType selectionType_ = SelectionType::None;

    // Panel layout
    float leftPanelWidth_ = 300.0f;
    bool isDraggingSplitter_ = false;

    // Timeline creation
    char newTimelineNameBuffer_[128] = "";

    // Track creation
    char trackNameBuffer_[128] = "";
    int selectedEntityIndex_ = 0;
    int selectedComponentIndex_ = 0;
    int selectedPropertyIndex_ = 0;
    std::vector<std::string> availableEntities_;
    std::vector<std::string> availableComponents_;
    std::vector<std::string> availableProperties_;

    // Graph editor state
    int selectedKeyframeIndex_ = -1;
    int draggingKeyframeIndex_ = -1;
    bool isDraggingKeyframe_ = false;
    float graphMinValue_ = -100.0f;
    float graphMaxValue_ = 100.0f;

    // Duration change confirmation
    bool showDurationConfirmPopup_ = false;
    float pendingDuration_ = 0.0f;

    // Helper methods
    void DrawTreeView();
    void DrawRightPanel();
    void DrawTimelineSettings();
    void DrawGraphicalKeyframeEditor();
    void DrawPlaybackControls();
    void DrawGraph(Track<float>* track, const ImVec2& graphMin, const ImVec2& graphSize);
    bool HandleGraphInput(Track<float>* track, const ImVec2& graphMin, const ImVec2& graphSize);
    int CountKeyframesOutsideDuration(float duration);
};

}  // namespace Elysium
