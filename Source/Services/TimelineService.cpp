#include "Services/TimelineService.h"
#include "Services/SceneService.h"
#include "Scene.h"
#include "Application.h"
#include "imgui.h"
#include "rlImGui.h"
#include <algorithm>
#include <cmath>

namespace Elysium::Services {

TimelineService::TimelineService() {
    name_ = "TimelineService";
}

void TimelineService::Initialize() {
}

void TimelineService::Shutdown() {
}

void TimelineService::Update(float deltaTime) {
}

void TimelineService::ImGui() {
    // Get current scene from SceneService
    auto& sceneService = Elysium::Application::GetInstance().GetService<SceneService>();
    currentScene_ = sceneService.GetScene();

    if (!currentScene_) {
        ImGui::Text("No scene loaded");
        return;
    }

    // Headers
    ImGui::Text("Timeline Hierarchy");
    ImGui::SameLine(leftPanelWidth_ + 10);
    if (selectionType_ == SelectionType::Timeline && selectedTimeline_) {
        ImGui::Text("Timeline: %s", selectedTimeline_->GetName().c_str());
    } else if (selectionType_ == SelectionType::Track && selectedTrack_) {
        ImGui::Text("Track: %s", selectedTrack_->GetName().c_str());
    } else if (selectionType_ == SelectionType::CreateTimeline) {
        ImGui::Text("Create Timeline");
    } else if (selectionType_ == SelectionType::CreateTrack) {
        ImGui::Text("Add Track");
    } else {
        ImGui::Text("Editor");
    }

    // Left panel - Tree View
    ImGui::BeginChild("TreeViewPanel", ImVec2(leftPanelWidth_, 0), true);
    DrawTreeView();
    ImGui::EndChild();

    ImGui::SameLine();

    // Splitter
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
    ImGui::Button("##splitter", ImVec2(4.0f, -1));

    if (ImGui::IsItemActive()) {
        if (!isDraggingSplitter_) {
            isDraggingSplitter_ = true;
        } else {
            leftPanelWidth_ += ImGui::GetIO().MouseDelta.x;
            if (leftPanelWidth_ < 200.0f)
                leftPanelWidth_ = 200.0f;
            if (leftPanelWidth_ > ImGui::GetWindowWidth() - 200.0f)
                leftPanelWidth_ = ImGui::GetWindowWidth() - 200.0f;
        }
    } else {
        isDraggingSplitter_ = false;
    }

    if (ImGui::IsItemHovered()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    }
    ImGui::PopStyleColor(3);
    ImGui::SameLine();

    // Right panel - Context-sensitive editor
    ImGui::BeginChild("EditorPanel", ImVec2(0, 0), true);
    DrawRightPanel();
    ImGui::EndChild();
}

void TimelineService::DrawTreeView() {
    // Playback toolbar
    if (selectedTimeline_) {
        if (ImGui::Button("Play")) {
            selectedTimeline_->Play();
        }
        ImGui::SameLine();
        if (ImGui::Button("Pause")) {
            selectedTimeline_->Pause();
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop")) {
            selectedTimeline_->Stop();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();

        // State indicator
        const char* stateStr = "Stopped";
        ImVec4 stateColor = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
        switch (selectedTimeline_->GetState()) {
            case TimelineState::Playing:
                stateStr = "Playing";
                stateColor = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
                break;
            case TimelineState::Paused:
                stateStr = "Paused";
                stateColor = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
                break;
            case TimelineState::Finished:
                stateStr = "Finished";
                stateColor = ImVec4(0.0f, 0.5f, 1.0f, 1.0f);
                break;
            default:
                break;
        }
        ImGui::TextColored(stateColor, "%s", stateStr);

        ImGui::Separator();
    }

    // Right-click on empty space to create timeline
    if (ImGui::BeginPopupContextWindow("TreeViewContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
        if (ImGui::MenuItem("Create Timeline")) {
            selectionType_ = SelectionType::CreateTimeline;
            selectedTimeline_ = nullptr;
            selectedTrack_ = nullptr;
            memset(newTimelineNameBuffer_, 0, sizeof(newTimelineNameBuffer_));
        }
        ImGui::EndPopup();
    }

    // Tree view of timelines and tracks
    const auto& timelines = currentScene_->GetTimelines();
    for (auto& timeline : timelines) {
        ImGui::PushID(timeline.get());

        // Timeline node
        ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
        if (selectedTimeline_ == timeline.get() && selectionType_ == SelectionType::Timeline) {
            nodeFlags |= ImGuiTreeNodeFlags_Selected;
        }

        // Timeline state indicators
        std::string timelineLabel = timeline->GetName();
        if (!timeline->IsPlaying() && !timeline->IsPaused()) {
            // Stopped/inactive
        } else if (timeline->IsPaused()) {
            timelineLabel = "[PAUSED] " + timelineLabel;
        } else if (timeline->IsPlaying()) {
            timelineLabel = "[PLAYING] " + timelineLabel;
        }

        bool nodeOpen = ImGui::TreeNodeEx(timelineLabel.c_str(), nodeFlags);

        // Click to select timeline
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
            selectedTimeline_ = timeline.get();
            selectedTrack_ = nullptr;
            selectionType_ = SelectionType::Timeline;
        }

        // Right-click menu on timeline
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Add Track")) {
                selectedTimeline_ = timeline.get();
                selectedTrack_ = nullptr;
                selectionType_ = SelectionType::CreateTrack;
                memset(trackNameBuffer_, 0, sizeof(trackNameBuffer_));

                // Populate entity list
                availableEntities_.clear();
                const auto& livingEntities = currentScene_->GetWorld()->GetLivingEntities();
                for (Entity entity : livingEntities) {
                    std::string name = currentScene_->GetWorld()->GetEntityName(entity);
                    if (name.empty()) {
                        name = "Entity_" + std::to_string(entity);
                    }
                    availableEntities_.push_back(name);
                }

                // Populate component list (hardcoded for now - could be dynamic)
                availableComponents_ = {"PositionComponent", "CameraComponent"};

                // Populate property list based on first component
                selectedEntityIndex_ = 0;
                selectedComponentIndex_ = 0;
                selectedPropertyIndex_ = 0;
                availableProperties_ = {"x", "y"};
            }
            if (ImGui::MenuItem("Edit Timeline")) {
                selectedTimeline_ = timeline.get();
                selectedTrack_ = nullptr;
                selectionType_ = SelectionType::CreateTimeline;
                strncpy(newTimelineNameBuffer_, timeline->GetName().c_str(), sizeof(newTimelineNameBuffer_));
            }
            if (ImGui::MenuItem("Delete Timeline")) {
                // Clear all pointers to this timeline
                if (selectedTimeline_ == timeline.get()) {
                    selectedTimeline_ = nullptr;
                    selectedTrack_ = nullptr;
                    selectionType_ = SelectionType::None;
                }
                currentScene_->RemoveTimeline(timeline->GetName());
            }
            ImGui::EndPopup();
        }

        // Show tracks as children
        if (nodeOpen) {
            const auto& tracks = timeline->GetTracks();
            for (auto& track : tracks) {
                ImGui::PushID(track.get());

                // Track label with entity and property info
                std::string trackLabel = track->GetName();
                if (!trackLabel.empty()) {
                    trackLabel += " ";
                }

                // Find entity name
                Entity targetEntity = track->GetTargetEntity();
                std::string entityName = currentScene_->GetWorld()->GetEntityName(targetEntity);
                if (entityName.empty()) {
                    entityName = "Entity_" + std::to_string(targetEntity);
                }

                trackLabel += "(" + entityName + ") - " +
                              track->GetComponentName() + "." + track->GetPropertyName();

                // Track flags
                ImGuiTreeNodeFlags trackFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                if (selectedTrack_ == track.get() && selectionType_ == SelectionType::Track) {
                    trackFlags |= ImGuiTreeNodeFlags_Selected;
                }

                // Active/inactive indicator
                if (!track->IsEnabled()) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
                    trackLabel = "[INACTIVE] " + trackLabel;
                }

                ImGui::TreeNodeEx(trackLabel.c_str(), trackFlags);

                if (!track->IsEnabled()) {
                    ImGui::PopStyleColor();
                }

                // Click to select track
                if (ImGui::IsItemClicked()) {
                    selectedTimeline_ = timeline.get();
                    selectedTrack_ = track.get();
                    selectionType_ = SelectionType::Track;
                    selectedKeyframeIndex_ = -1;
                }

                // Right-click menu on track
                if (ImGui::BeginPopupContextItem()) {
                    bool enabled = track->IsEnabled();
                    if (ImGui::MenuItem(enabled ? "Disable" : "Enable")) {
                        track->SetEnabled(!enabled);
                    }
                    if (ImGui::MenuItem("Delete Track")) {
                        timeline->RemoveTrack(track->GetName());
                        if (selectedTrack_ == track.get()) {
                            selectedTrack_ = nullptr;
                            selectionType_ = SelectionType::None;
                        }
                    }
                    ImGui::EndPopup();
                }

                ImGui::PopID();
            }
            ImGui::TreePop();
        }

        ImGui::PopID();
    }
}

void TimelineService::DrawRightPanel() {
    if (selectionType_ == SelectionType::CreateTimeline) {
        bool isEditing = selectedTimeline_ != nullptr;

        // Create/Edit timeline form
        ImGui::Text(isEditing ? "Edit Timeline" : "Create New Timeline");
        ImGui::Separator();

        ImGui::InputTextWithHint("##TimelineName", "Timeline name...", newTimelineNameBuffer_, sizeof(newTimelineNameBuffer_));

        if (ImGui::Button(isEditing ? "Save##SaveTimelineBtn" : "Create##CreateTimelineBtn")) {
            if (strlen(newTimelineNameBuffer_) > 0) {
                if (isEditing) {
                    // Rename existing timeline
                    selectedTimeline_->SetName(newTimelineNameBuffer_);
                    selectionType_ = SelectionType::Timeline;
                } else {
                    // Create new timeline
                    auto* newTimeline = currentScene_->CreateTimeline(newTimelineNameBuffer_);
                    selectedTimeline_ = newTimeline;
                    selectionType_ = SelectionType::Timeline;
                }
                memset(newTimelineNameBuffer_, 0, sizeof(newTimelineNameBuffer_));
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel##CancelCreateTimeline")) {
            if (isEditing) {
                selectionType_ = SelectionType::Timeline;
            } else {
                selectionType_ = SelectionType::None;
                selectedTimeline_ = nullptr;
            }
        }
    } else if (selectionType_ == SelectionType::CreateTrack && selectedTimeline_) {
        // Create track form
        ImGui::Text("Add Track to: %s", selectedTimeline_->GetName().c_str());
        ImGui::Separator();

        ImGui::InputTextWithHint("##TrackName", "Track name (optional)", trackNameBuffer_, sizeof(trackNameBuffer_));

        // Entity combo box
        if (!availableEntities_.empty()) {
            if (ImGui::BeginCombo("Entity", availableEntities_[selectedEntityIndex_].c_str())) {
                for (int i = 0; i < availableEntities_.size(); ++i) {
                    bool isSelected = (selectedEntityIndex_ == i);
                    if (ImGui::Selectable(availableEntities_[i].c_str(), isSelected)) {
                        selectedEntityIndex_ = i;
                    }
                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
        } else {
            ImGui::TextDisabled("No entities in scene");
        }

        // Component combo box
        int oldComponentIndex = selectedComponentIndex_;
        if (ImGui::BeginCombo("Component", availableComponents_[selectedComponentIndex_].c_str())) {
            for (int i = 0; i < availableComponents_.size(); ++i) {
                bool isSelected = (selectedComponentIndex_ == i);
                if (ImGui::Selectable(availableComponents_[i].c_str(), isSelected)) {
                    selectedComponentIndex_ = i;
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        // Update properties when component changes
        if (oldComponentIndex != selectedComponentIndex_) {
            selectedPropertyIndex_ = 0;
            if (availableComponents_[selectedComponentIndex_] == "PositionComponent") {
                availableProperties_ = {"x", "y"};
            } else if (availableComponents_[selectedComponentIndex_] == "CameraComponent") {
                availableProperties_ = {"zoom"};
            }
        }

        // Property combo box
        if (!availableProperties_.empty()) {
            if (ImGui::BeginCombo("Property", availableProperties_[selectedPropertyIndex_].c_str())) {
                for (int i = 0; i < availableProperties_.size(); ++i) {
                    bool isSelected = (selectedPropertyIndex_ == i);
                    if (ImGui::Selectable(availableProperties_[i].c_str(), isSelected)) {
                        selectedPropertyIndex_ = i;
                    }
                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
        }

        if (ImGui::Button("Create##CreateTrackBtn")) {
            if (!availableEntities_.empty()) {
                const std::string& entityName = availableEntities_[selectedEntityIndex_];
                const std::string& componentName = availableComponents_[selectedComponentIndex_];
                const std::string& propertyName = availableProperties_[selectedPropertyIndex_];

                Entity entity = INVALID_ENTITY;
                if (currentScene_->GetWorld()->GetEntityByName(entityName, &entity)) {
                    auto* newTrack = selectedTimeline_->AddTrack<float>(
                        trackNameBuffer_,
                        entity,
                        componentName,
                        propertyName
                    );
                    selectedTrack_ = newTrack;
                    selectionType_ = SelectionType::Track;
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel##CancelCreateTrack")) {
            selectionType_ = SelectionType::Timeline;
            selectedTrack_ = nullptr;
        }
    } else if (selectionType_ == SelectionType::Timeline && selectedTimeline_) {
        DrawTimelineSettings();
    } else if (selectionType_ == SelectionType::Track && selectedTrack_) {
        DrawGraphicalKeyframeEditor();
    } else {
        ImGui::TextDisabled("Right-click in the tree to create a timeline");
    }
}

void TimelineService::DrawTimelineSettings() {
    if (!selectedTimeline_) return;

    DrawPlaybackControls();

    ImGui::Separator();

    // Timeline properties
    float duration = selectedTimeline_->GetDuration();
    if (ImGui::InputFloat("Duration (seconds)", &duration, 0.1f, 1.0f)) {
        if (duration >= 0.0f) {
            // Check if this would cull keyframes
            int cullCount = CountKeyframesOutsideDuration(duration);
            if (cullCount > 0) {
                // Show confirmation popup
                pendingDuration_ = duration;
                showDurationConfirmPopup_ = true;
            } else {
                // Safe to change
                selectedTimeline_->SetDuration(duration);
            }
        }
    }

    // Duration change confirmation popup
    if (showDurationConfirmPopup_) {
        ImGui::OpenPopup("Confirm Duration Change");
        showDurationConfirmPopup_ = false;
    }

    if (ImGui::BeginPopupModal("Confirm Duration Change", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        int cullCount = CountKeyframesOutsideDuration(pendingDuration_);
        ImGui::Text("Changing duration to %.2fs will remove %d keyframe(s).", pendingDuration_, cullCount);
        ImGui::Text("This action cannot be undone.");
        ImGui::Separator();

        if (ImGui::Button("OK##ConfirmDurationChange", ImVec2(120, 0))) {
            // Apply duration and cull keyframes
            selectedTimeline_->SetDuration(pendingDuration_);

            // Cull keyframes from all tracks
            for (const auto& trackPtr : selectedTimeline_->GetTracks()) {
                auto* floatTrack = dynamic_cast<Track<float>*>(trackPtr.get());
                if (floatTrack) {
                    auto& keyframes = floatTrack->GetKeyframes();
                    keyframes.erase(
                        std::remove_if(keyframes.begin(), keyframes.end(),
                            [this](const Keyframe<float>& kf) { return kf.time > pendingDuration_; }),
                        keyframes.end()
                    );
                }
            }

            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel##CancelDurationChange", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    float speed = selectedTimeline_->GetPlaybackSpeed();
    if (ImGui::SliderFloat("Playback Speed", &speed, 0.1f, 3.0f)) {
        selectedTimeline_->SetPlaybackSpeed(speed);
    }

    bool loop = selectedTimeline_->IsLooping();
    if (ImGui::Checkbox("Loop", &loop)) {
        selectedTimeline_->SetLoop(loop);
    }

    ImGui::Separator();
    ImGui::Text("Tracks: %zu", selectedTimeline_->GetTracks().size());
}

void TimelineService::DrawPlaybackControls() {
    if (!selectedTimeline_) return;

    // Playback buttons
    if (ImGui::Button("Play##PlayBtn")) {
        selectedTimeline_->Play();
    }
    ImGui::SameLine();
    if (ImGui::Button("Pause##PauseBtn")) {
        selectedTimeline_->Pause();
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop##StopBtn")) {
        selectedTimeline_->Stop();
    }

    // Timeline state
    const char* stateStr = "Unknown";
    ImVec4 stateColor = ImVec4(1, 1, 1, 1);
    switch (selectedTimeline_->GetState()) {
        case TimelineState::Stopped:
            stateStr = "Stopped";
            stateColor = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
            break;
        case TimelineState::Playing:
            stateStr = "Playing";
            stateColor = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
            break;
        case TimelineState::Paused:
            stateStr = "Paused";
            stateColor = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
            break;
        case TimelineState::Finished:
            stateStr = "Finished";
            stateColor = ImVec4(0.0f, 0.5f, 1.0f, 1.0f);
            break;
    }
    ImGui::TextColored(stateColor, "State: %s", stateStr);

    // Current time / duration
    float currentTime = selectedTimeline_->GetCurrentTime();
    float duration = selectedTimeline_->GetDuration();
    ImGui::Text("Time: %.2f / %.2f", currentTime, duration);

    // Seek slider
    if (duration > 0.0f) {
        if (ImGui::SliderFloat("##Seek", &currentTime, 0.0f, duration)) {
            selectedTimeline_->Seek(currentTime);
        }
    }
}

void TimelineService::DrawGraphicalKeyframeEditor() {
    if (!selectedTrack_) return;

    // Try to cast to float track (only type we support)
    auto* floatTrack = dynamic_cast<Track<float>*>(selectedTrack_);
    if (!floatTrack) {
        ImGui::Text("Unsupported track type (only float tracks supported)");
        return;
    }

    // Track info
    ImGui::Text("Entity: %s", currentScene_->GetWorld()->GetEntityName(selectedTrack_->GetTargetEntity()).c_str());
    ImGui::Text("Property: %s.%s", selectedTrack_->GetComponentName().c_str(), selectedTrack_->GetPropertyName().c_str());
    ImGui::Text("Keyframes: %zu", floatTrack->GetKeyframes().size());

    ImGui::Separator();

    // Graph range controls
    ImGui::DragFloat("Min Value", &graphMinValue_, 1.0f);
    ImGui::DragFloat("Max Value", &graphMaxValue_, 1.0f);

    if (ImGui::Button("Auto-fit Range")) {
        auto& keyframes = floatTrack->GetKeyframes();
        if (!keyframes.empty()) {
            graphMinValue_ = keyframes[0].value;
            graphMaxValue_ = keyframes[0].value;
            for (const auto& kf : keyframes) {
                graphMinValue_ = std::min(graphMinValue_, kf.value);
                graphMaxValue_ = std::max(graphMaxValue_, kf.value);
            }
            // Add 10% padding
            float range = graphMaxValue_ - graphMinValue_;
            if (range < 0.001f) range = 10.0f;
            graphMinValue_ -= range * 0.1f;
            graphMaxValue_ += range * 0.1f;
        }
    }

    ImGui::Separator();

    // Instructions
    ImGui::TextDisabled("Left-click: Add/Select keyframe | Right-click: Delete keyframe | Drag: Move keyframe");

    ImGui::Separator();

    // Graph area
    ImVec2 graphSize = ImGui::GetContentRegionAvail();
    graphSize.y = std::max(graphSize.y - 50.0f, 200.0f); // Leave space for instructions

    ImVec2 graphMin = ImGui::GetCursorScreenPos();

    // Use InvisibleButton to capture input and prevent window dragging
    ImGui::InvisibleButton("##GraphCanvas", graphSize, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
    bool isGraphHovered = ImGui::IsItemHovered();
    bool isGraphActive = ImGui::IsItemActive();

    // Draw graph
    DrawGraph(floatTrack, graphMin, graphSize);

    // Handle input only if we're hovering or actively dragging
    if (isGraphHovered || isDraggingKeyframe_) {
        HandleGraphInput(floatTrack, graphMin, graphSize);
    }
}

void TimelineService::DrawGraph(Elysium::Track<float>* track, const ImVec2& graphMin, const ImVec2& graphSize) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    ImVec2 graphMax = ImVec2(graphMin.x + graphSize.x, graphMin.y + graphSize.y);

    // Background
    drawList->AddRectFilled(graphMin, graphMax, IM_COL32(30, 30, 30, 255));
    drawList->AddRect(graphMin, graphMax, IM_COL32(100, 100, 100, 255));

    // Get timeline duration
    float duration = selectedTimeline_ ? selectedTimeline_->GetDuration() : 10.0f;
    if (duration < 0.001f) duration = 10.0f;

    // Draw grid
    int numVerticalLines = 10;
    int numHorizontalLines = 5;

    for (int i = 0; i <= numVerticalLines; ++i) {
        float x = graphMin.x + (graphSize.x * i / numVerticalLines);
        drawList->AddLine(ImVec2(x, graphMin.y), ImVec2(x, graphMax.y), IM_COL32(60, 60, 60, 255));

        // Time labels
        float time = duration * i / numVerticalLines;
        char label[32];
        snprintf(label, sizeof(label), "%.1fs", time);
        drawList->AddText(ImVec2(x + 2, graphMax.y + 2), IM_COL32(150, 150, 150, 255), label);
    }

    for (int i = 0; i <= numHorizontalLines; ++i) {
        float y = graphMin.y + (graphSize.y * i / numHorizontalLines);
        drawList->AddLine(ImVec2(graphMin.x, y), ImVec2(graphMax.x, y), IM_COL32(60, 60, 60, 255));

        // Value labels
        float value = graphMaxValue_ - (graphMaxValue_ - graphMinValue_) * i / numHorizontalLines;
        char label[32];
        snprintf(label, sizeof(label), "%.1f", value);
        drawList->AddText(ImVec2(graphMin.x + 2, y - 8), IM_COL32(150, 150, 150, 255), label);
    }

    // Draw current time indicator
    if (selectedTimeline_) {
        float currentTime = selectedTimeline_->GetCurrentTime();
        float x = graphMin.x + (currentTime / duration) * graphSize.x;
        drawList->AddLine(ImVec2(x, graphMin.y), ImVec2(x, graphMax.y), IM_COL32(255, 255, 0, 200), 2.0f);
    }

    // Draw curve between keyframes
    auto& keyframes = track->GetKeyframes();
    if (keyframes.size() >= 2) {
        for (size_t i = 0; i < keyframes.size() - 1; ++i) {
            const auto& kf1 = keyframes[i];
            const auto& kf2 = keyframes[i + 1];

            float x1 = graphMin.x + (kf1.time / duration) * graphSize.x;
            float y1 = graphMax.y - ((kf1.value - graphMinValue_) / (graphMaxValue_ - graphMinValue_)) * graphSize.y;
            float x2 = graphMin.x + (kf2.time / duration) * graphSize.x;
            float y2 = graphMax.y - ((kf2.value - graphMinValue_) / (graphMaxValue_ - graphMinValue_)) * graphSize.y;

            // Clamp to graph bounds
            y1 = std::max(graphMin.y, std::min(graphMax.y, y1));
            y2 = std::max(graphMin.y, std::min(graphMax.y, y2));

            drawList->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), IM_COL32(100, 200, 255, 255), 2.0f);
        }
    }

    // Draw keyframe points
    for (size_t i = 0; i < keyframes.size(); ++i) {
        const auto& kf = keyframes[i];

        float x = graphMin.x + (kf.time / duration) * graphSize.x;
        float y = graphMax.y - ((kf.value - graphMinValue_) / (graphMaxValue_ - graphMinValue_)) * graphSize.y;

        // Clamp to graph bounds
        y = std::max(graphMin.y, std::min(graphMax.y, y));

        ImU32 color = (i == selectedKeyframeIndex_) ? IM_COL32(255, 255, 0, 255) : IM_COL32(255, 100, 100, 255);
        drawList->AddCircleFilled(ImVec2(x, y), 5.0f, color);
        drawList->AddCircle(ImVec2(x, y), 5.0f, IM_COL32(255, 255, 255, 255), 12, 1.5f);
    }
}

bool TimelineService::HandleGraphInput(Elysium::Track<float>* track, const ImVec2& graphMin, const ImVec2& graphSize) {
    ImVec2 graphMax = ImVec2(graphMin.x + graphSize.x, graphMin.y + graphSize.y);
    ImVec2 mousePos = ImGui::GetMousePos();

    // Check if mouse is in graph
    bool inGraph = mousePos.x >= graphMin.x && mousePos.x <= graphMax.x &&
                   mousePos.y >= graphMin.y && mousePos.y <= graphMax.y;

    if (!inGraph && !isDraggingKeyframe_) {
        return false;
    }

    float duration = selectedTimeline_ ? selectedTimeline_->GetDuration() : 10.0f;
    if (duration < 0.001f) duration = 10.0f;

    auto& keyframes = track->GetKeyframes();

    // Left-click: Add or select keyframe
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && inGraph) {
        // Check if clicking existing keyframe
        bool clickedExisting = false;
        for (size_t i = 0; i < keyframes.size(); ++i) {
            const auto& kf = keyframes[i];
            float x = graphMin.x + (kf.time / duration) * graphSize.x;
            float y = graphMax.y - ((kf.value - graphMinValue_) / (graphMaxValue_ - graphMinValue_)) * graphSize.y;

            float dx = mousePos.x - x;
            float dy = mousePos.y - y;
            float dist = sqrt(dx * dx + dy * dy);

            if (dist < 8.0f) {
                selectedKeyframeIndex_ = i;
                draggingKeyframeIndex_ = i;
                isDraggingKeyframe_ = true;
                clickedExisting = true;
                break;
            }
        }

        // Add new keyframe
        if (!clickedExisting) {
            float time = ((mousePos.x - graphMin.x) / graphSize.x) * duration;
            float value = graphMaxValue_ - ((mousePos.y - graphMin.y) / graphSize.y) * (graphMaxValue_ - graphMinValue_);

            time = std::max(0.0f, std::min(duration, time));

            track->AddKeyframe(time, value);
        }
    }

    // Dragging keyframe
    if (isDraggingKeyframe_ && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        if (draggingKeyframeIndex_ >= 0 && draggingKeyframeIndex_ < keyframes.size()) {
            auto& kf = keyframes[draggingKeyframeIndex_];

            float time = ((mousePos.x - graphMin.x) / graphSize.x) * duration;
            float value = graphMaxValue_ - ((mousePos.y - graphMin.y) / graphSize.y) * (graphMaxValue_ - graphMinValue_);

            kf.time = std::max(0.0f, std::min(duration, time));
            kf.value = value;

            // Re-sort keyframes by time
            std::sort(keyframes.begin(), keyframes.end(),
                [](const Keyframe<float>& a, const Keyframe<float>& b) { return a.time < b.time; });
        }
    }

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        isDraggingKeyframe_ = false;
        draggingKeyframeIndex_ = -1;
    }

    // Right-click: Delete keyframe
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && inGraph) {
        for (size_t i = 0; i < keyframes.size(); ++i) {
            const auto& kf = keyframes[i];
            float x = graphMin.x + (kf.time / duration) * graphSize.x;
            float y = graphMax.y - ((kf.value - graphMinValue_) / (graphMaxValue_ - graphMinValue_)) * graphSize.y;

            float dx = mousePos.x - x;
            float dy = mousePos.y - y;
            float dist = sqrt(dx * dx + dy * dy);

            if (dist < 8.0f) {
                track->RemoveKeyframe(i);
                if (selectedKeyframeIndex_ == i) {
                    selectedKeyframeIndex_ = -1;
                }
                break;
            }
        }
    }

    return true;
}

int TimelineService::CountKeyframesOutsideDuration(float duration) {
    if (!selectedTimeline_) return 0;

    int count = 0;
    for (const auto& trackPtr : selectedTimeline_->GetTracks()) {
        auto* floatTrack = dynamic_cast<Track<float>*>(trackPtr.get());
        if (floatTrack) {
            for (const auto& kf : floatTrack->GetKeyframes()) {
                if (kf.time > duration) {
                    count++;
                }
            }
        }
    }
    return count;
}

} // namespace Elysium::Services
