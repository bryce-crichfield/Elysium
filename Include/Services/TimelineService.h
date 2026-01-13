#pragma once

#include "Service.h"
#include "Timeline.h"
#include "Entity.h"
#include <string>
#include <memory>

namespace Elysium {
    class Scene;
}

namespace Elysium::Services {

class TimelineService : public Elysium::Service {
private:
    Elysium::Scene* currentScene_ = nullptr;
    Elysium::Timeline* selectedTimeline_ = nullptr;
    Elysium::TrackBase* selectedTrack_ = nullptr;

    enum class SelectionType { None, Timeline, Track, CreateTimeline, CreateTrack };
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

    // Graph editor helpers
    void DrawGraph(Elysium::Track<float>* track, const ImVec2& graphMin, const ImVec2& graphSize);
    bool HandleGraphInput(Elysium::Track<float>* track, const ImVec2& graphMin, const ImVec2& graphSize);

    // Helper to count keyframes outside duration
    int CountKeyframesOutsideDuration(float duration);

public:
    TimelineService();
    ~TimelineService() = default;

    // Service interface
    void Initialize() override;
    void Shutdown() override;
    void Update(float deltaTime) override;
    void ImGui() override;

    // Set current scene to edit timelines for
    void SetCurrentScene(Elysium::Scene* scene) { currentScene_ = scene; }
};

} // namespace Elysium::Services
