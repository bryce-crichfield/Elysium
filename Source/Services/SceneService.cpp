#include "Services/SceneService.h"
#include "Services/LogService.h"
#include "imgui.h"
#include "raylib.h"
#include "tinyxml2.h"

using namespace tinyxml2;

namespace Elysium::Services
{

// =============================================================================
// Constructor & State Machine Setup
// =============================================================================

SceneService::SceneService() {
    InitializeStateMachine();
}

void SceneService::InitializeStateMachine() {
    // Scene transition states
    constexpr auto NONE = "NONE";
    constexpr auto EXITING = "EXITING";
    constexpr auto LOADING_ASSETS = "LOADING_ASSETS";
    constexpr auto ASSETS_LOADED = "ASSETS_LOADED";
    constexpr auto ENTERING = "ENTERING";
    constexpr auto ACTIVE = "ACTIVE";
    constexpr auto ACTIVE_RUNNING = "ACTIVE_RUNNING";
    constexpr auto ACTIVE_PAUSED = "ACTIVE_PAUSED";

    // Valid state transitions: NONE/ACTIVE_* -> EXITING -> LOADING_ASSETS -> ASSETS_LOADED -> ENTERING -> ACTIVE -> ACTIVE_RUNNING <-> ACTIVE_PAUSED
    transitionStateMachine_.AddValidTransition(NONE, EXITING);
    transitionStateMachine_.AddValidTransition(NONE, LOADING_ASSETS);
    transitionStateMachine_.AddValidTransition(ACTIVE_RUNNING, EXITING);
    transitionStateMachine_.AddValidTransition(ACTIVE_PAUSED, EXITING);
    transitionStateMachine_.AddValidTransition(EXITING, LOADING_ASSETS);
    transitionStateMachine_.AddValidTransition(LOADING_ASSETS, ASSETS_LOADED);
    transitionStateMachine_.AddValidTransition(ASSETS_LOADED, ENTERING);
    transitionStateMachine_.AddValidTransition(ENTERING, ACTIVE);
    transitionStateMachine_.AddValidTransition(ACTIVE, ACTIVE_RUNNING);
    transitionStateMachine_.AddValidTransition(ACTIVE_RUNNING, ACTIVE_PAUSED);
    transitionStateMachine_.AddValidTransition(ACTIVE_PAUSED, ACTIVE_RUNNING);

    // State event handlers
    transitionStateMachine_.RegisterOnEnter(EXITING, [this]() { OnEnterExiting(); });
    transitionStateMachine_.RegisterOnUpdate(EXITING, [this]() { OnUpdateExiting(); });
    transitionStateMachine_.RegisterOnEnter(LOADING_ASSETS, [this]() { OnEnterLoadingAssets(); });
    transitionStateMachine_.RegisterOnEnter(ASSETS_LOADED, [this]() { OnEnterAssetsLoaded(); });
    transitionStateMachine_.RegisterOnEnter(ENTERING, [this]() { OnEnterEntering(); });
    transitionStateMachine_.RegisterOnUpdate(ENTERING, [this]() { OnUpdateEntering(); });
    transitionStateMachine_.RegisterOnEnter(ACTIVE, [this]() { OnEnterActive(); });
    transitionStateMachine_.RegisterOnEnter(ACTIVE_RUNNING, [this]() { OnEnterActiveRunning(); });
    transitionStateMachine_.RegisterOnEnter(ACTIVE_PAUSED, [this]() { OnEnterActivePaused(); });

    transitionStateMachine_.SetCurrentState(NONE);
}

// =============================================================================
// State Machine Event Handlers
// =============================================================================

void SceneService::OnEnterExiting() {
    transitionTimer_ = 0.0f;
    transitionDuration_ = (sceneQueue_.size() > 1) ? 0.1f : 1.0f;
}

void SceneService::OnUpdateExiting() {
    transitionTimer_ += GetFrameTime();
    if (transitionTimer_ >= transitionDuration_) {
        transitionStateMachine_.TransitionTo("LOADING_ASSETS");
    }
}

void SceneService::OnEnterLoadingAssets() {
    transitionTimer_ = 0.0f;

    if (!sceneQueue_.empty()) {
        const std::string& sceneName = sceneQueue_.front();
        UpdateSceneStatus(sceneName, SceneStatus::LOADING_ASSETS);

        Scene* scene = CreateOrGetScene(sceneName);
        if (scene) {
            pendingAssets_ = scene->GetAssets();
            LOG_INFOF("SceneService", "Scene '%s' requested %zu assets for loading", sceneName.c_str(), pendingAssets_.size());
            for (const auto& asset : pendingAssets_) {
                LOG_DEBUGF("SceneService", "Asset requested: %s (%s) -> %s",
                          asset.GetName().c_str(),
                          asset.GetType() == AssetType::TEXTURE ? "TEXTURE" :
                          asset.GetType() == AssetType::SOUND ? "SOUND" :
                          asset.GetType() == AssetType::MUSIC ? "MUSIC" :
                          asset.GetType() == AssetType::FONT ? "FONT" :
                          asset.GetType() == AssetType::MODEL ? "MODEL" :
                          asset.GetType() == AssetType::SHADER ? "SHADER" :
                          asset.GetType() == AssetType::SPRITE ? "SPRITE" : "UNKNOWN",
                          asset.GetPath().c_str());
            }
        } else {
            LOG_ERRORF("SceneService", "Failed to create/get scene '%s' for asset loading", sceneName.c_str());
        }
    }
}

void SceneService::OnEnterAssetsLoaded() {
    if (!sceneQueue_.empty()) {
        UpdateSceneStatus(sceneQueue_.front(), SceneStatus::ASSETS_LOADED);
    }
    transitionStateMachine_.TransitionTo("ENTERING");
}

void SceneService::OnEnterEntering() {
    transitionTimer_ = 0.0f;
    if (!sceneQueue_.empty()) {
        UpdateSceneStatus(sceneQueue_.front(), SceneStatus::ENTERING);
    }
}

void SceneService::OnUpdateEntering() {
    transitionTimer_ += GetFrameTime();
    if (transitionTimer_ >= transitionDuration_) {
        transitionStateMachine_.TransitionTo("ACTIVE");
    }
}

void SceneService::OnEnterActive() {
    if (!sceneQueue_.empty()) {
        const std::string sceneName = sceneQueue_.front();
        sceneQueue_.pop();

        // Cleanup old scene
        if (activeScene_) {
            activeScene_->OnExit();
            UpdateActiveSceneStatus(SceneStatus::SUSPENDED);
        }

        // Activate new scene
        activeScene_ = CreateOrGetScene(sceneName);
        if (activeScene_) {
            EnterScene(sceneName);
            UpdateSceneStatus(sceneName, SceneStatus::ACTIVE);
        }
    }

    // Automatically transition to ACTIVE_RUNNING
    transitionStateMachine_.TransitionTo("ACTIVE_RUNNING");
}

void SceneService::OnEnterActiveRunning() {
    // Scene is now running - updates will be called
}

void SceneService::OnEnterActivePaused() {
    // Scene is paused - updates will not be called, but rendering continues
}

// =============================================================================
// Helper Methods
// =============================================================================

void SceneService::UpdateSceneStatus(const std::string& name, SceneStatus status) {
    auto it = scenes_.find(name);
    if (it != scenes_.end()) {
        it->second.status = status;
    }
}

void SceneService::UpdateActiveSceneStatus(SceneStatus status) {
    for (auto& [name, data] : scenes_) {
        if (data.scene == activeScene_) {
            data.status = status;
            break;
        }
    }
}

std::string PrintSceneStatus(SceneStatus status) {
    switch (status) {
        case SceneStatus::UNLOADED: return "UNLOADED";
        case SceneStatus::LOADING_ASSETS: return "LOADING_ASSETS";
        case SceneStatus::ASSETS_LOADED: return "ASSETS_LOADED";
        case SceneStatus::INITIALIZING: return "INITIALIZING";
        case SceneStatus::ENTERING: return "ENTERING";
        case SceneStatus::ACTIVE: return "ACTIVE";
        case SceneStatus::EXITING: return "EXITING";
        case SceneStatus::SUSPENDED: return "SUSPENDED";
    }
    return "UNKNOWN";
}

// =============================================================================
// Public Interface
// =============================================================================

Scene* SceneService::GetScene() const {
    return activeScene_;
}

bool SceneService::IsTransitioning() const {
    const std::string& state = transitionStateMachine_.GetCurrentState();
    return state != "NONE" && state != "ACTIVE_RUNNING" && state != "ACTIVE_PAUSED";
}

void SceneService::SetScene(std::string name) {
    // Clear queue and force immediate scene transition
    while (!sceneQueue_.empty()) {
        sceneQueue_.pop();
    }

    sceneQueue_.push(name);
    transitionDuration_ = 0.0f; // Immediate transition

    if (activeScene_) {
        transitionStateMachine_.TransitionTo("EXITING");
    } else {
        transitionStateMachine_.TransitionTo("LOADING_ASSETS");
    }
}

void SceneService::RegisterScene(const std::string& name, std::string xmlPath, SceneFactory factory) {
    SceneData data{name, nullptr, factory, {}, xmlPath, false, SceneStatus::UNLOADED};
    scenes_.emplace(name, data);
    LOG_INFOF("SceneService", "Registered scene: %s", name.c_str());
}

void SceneService::QueueScene(std::string name) {
    auto it = scenes_.find(name);
    if (it == scenes_.end()) {
        LOG_ERRORF("SceneService", "Scene not found: %s", name.c_str());
        return;
    }

    sceneQueue_.push(name);
    LOG_INFOF("SceneService", "Queued scene: %s", name.c_str());
}

void SceneService::Update(float deltaTime) {
    // Cache deltaTime for smooth pause/unpause transitions
    if (deltaTime > 0.0f && deltaTime < 0.1f) { // Reasonable deltaTime bounds
        cachedDeltaTime_ = deltaTime;
    }

    const std::string& currentState = transitionStateMachine_.GetCurrentState();

    // Start transition if we have queued scenes and can transition
    if (!sceneQueue_.empty() && (currentState == "NONE" || currentState == "ACTIVE_RUNNING" || currentState == "ACTIVE_PAUSED")) {
        if (activeScene_ && (currentState == "ACTIVE_RUNNING" || currentState == "ACTIVE_PAUSED")) {
            transitionStateMachine_.TransitionTo("EXITING");
        } else if (!activeScene_ && currentState == "NONE") {
            transitionStateMachine_.TransitionTo("LOADING_ASSETS");
        }
    }

    transitionStateMachine_.Update();

    // Update active scene when running
    if (activeScene_ && currentState == "ACTIVE_RUNNING") {
        activeScene_->OnUpdate(cachedDeltaTime_);

        // Handle timeout if active
        if (isTimingOut_) {
            timeoutTimer_ += cachedDeltaTime_ * 1000.0f; // Convert to milliseconds

            // Check if timeout duration has been reached
            if (timeoutTimer_ >= timeoutDuration_) {
                isTimingOut_ = false;
                timeoutTimer_ = 0.0f;
                transitionStateMachine_.TransitionTo("ACTIVE_PAUSED");
            }
        }
    }
}


// =============================================================================
// Scene Management
// =============================================================================

Scene* SceneService::CreateOrGetScene(const std::string& name) {
    auto it = scenes_.find(name);
    if (it == scenes_.end()) {
        LOG_ERRORF("SceneService", "Scene not found: %s", name.c_str());
        return nullptr;
    }

    SceneData& sceneData = it->second;
    if (!sceneData.scene) {
        sceneData.scene = sceneData.factory();
    }

    return sceneData.scene;
}

void SceneService::EnterScene(const std::string& name) {
    auto it = scenes_.find(name);
    if (it == scenes_.end()) {
        LOG_ERRORF("SceneService", "Cannot enter scene. Scene not found: %s", name.c_str());
        return;
    }

    SceneData& sceneData = it->second;
    Scene* scene = CreateOrGetScene(name);

    if (scene) {
        sceneData.status = SceneStatus::INITIALIZING;
        if (!sceneData.xmlPath.empty() && !sceneData.xmlLoaded) {
            sceneData.scene->LoadFromXML(sceneData.xmlPath);
            sceneData.xmlLoaded = true;
        }
        sceneData.status = SceneStatus::ENTERING;
        scene->OnEnter();
    }
}

// =============================================================================
// Debug & Utility
// =============================================================================

void SceneService::DebugDraw() {
    if (!inspectorVisible_) return;

    ImGui::Begin("Scene Service", &inspectorVisible_, ImGuiWindowFlags_NoCollapse);

    // Active scene info and controls
    if (activeScene_) {
        std::string activeSceneName = "Unknown";
        for (const auto& [name, data] : scenes_) {
            if (data.scene == activeScene_) {
                activeSceneName = name;
                break;
            }
        }
        ImGui::Text("Active Scene: %s", activeSceneName.c_str());
        ImGui::SameLine();

        // Play/Pause/Step controls
        const std::string& currentState = transitionStateMachine_.GetCurrentState();
        if (currentState == "ACTIVE_RUNNING") {
            if (ImGui::Button("Pause")) {
                transitionStateMachine_.TransitionTo("ACTIVE_PAUSED");
            }
        } else if (currentState == "ACTIVE_PAUSED") {
            if (ImGui::Button("Play")) {
                transitionStateMachine_.TransitionTo("ACTIVE_RUNNING");
            }
            ImGui::SameLine();

            // Timeout configuration
            ImGui::PushItemWidth(80);
            ImGui::InputFloat("##timeout", &timeoutDuration_, 10.0f, 100.0f, "%.0f");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Timeout duration in milliseconds\nRun for this duration then pause");
            }
            ImGui::PopItemWidth();
            if (timeoutDuration_ < 1.0f) timeoutDuration_ = 1.0f;

            ImGui::SameLine();
            ImGui::Text("ms");
            ImGui::SameLine();

            if (ImGui::Button("Timeout")) {
                StartTimeout();
            }

            // Show timeout progress if currently timing out
            if (isTimingOut_) {
                ImGui::SameLine();
                float remaining = timeoutDuration_ - timeoutTimer_;
                ImGui::Text("(%.0fms left)", remaining);
            }
        }

        ImGui::Text("State: %s", currentState.c_str());
        if (IsTransitioning()) {
            ImGui::Text("Progress: %.2f", GetTransitionProgress());
        }
    } else {
        ImGui::Text("No active scene");
    }

    ImGui::Separator();

    // Scene Management Buttons
    if (ImGui::Button("Create")) {
        // TODO: Implement scene creation
    }
    ImGui::SameLine();

    static int selectedSceneIndex = -1;
    bool hasSelection = selectedSceneIndex >= 0 && selectedSceneIndex < scenes_.size();

    if (ImGui::Button("Load") && hasSelection) {
        auto it = scenes_.begin();
        std::advance(it, selectedSceneIndex);
        QueueScene(it->first);
    }
    if (!hasSelection) {
        ImGui::SetItemTooltip("Select a scene to load");
    }

    ImGui::SameLine();
    if (ImGui::Button("Remove") && hasSelection) {
        // TODO: Implement scene removal
    }
    if (!hasSelection) {
        ImGui::SetItemTooltip("Select a scene to remove");
    }

    ImGui::Separator();

    // Scene Registry Table
    if (ImGui::BeginTable("SceneTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Status");
        ImGui::TableSetupColumn("XML Path");
        ImGui::TableSetupColumn("Assets");
        ImGui::TableHeadersRow();

        int index = 0;
        for (const auto& [name, sceneData] : scenes_) {
            ImGui::TableNextRow();

            // Name column with selection
            ImGui::TableSetColumnIndex(0);
            if (ImGui::Selectable(name.c_str(), selectedSceneIndex == index, ImGuiSelectableFlags_SpanAllColumns)) {
                selectedSceneIndex = index;
            }

            // Status column
            ImGui::TableSetColumnIndex(1);
            std::string statusText = PrintSceneStatus(sceneData.status);
            ImGui::Text("%s", statusText.c_str());

            // XML Path column
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%s", sceneData.xmlPath.c_str());

            // Assets column
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%zu assets", sceneData.assets.size());

            index++;
        }
        ImGui::EndTable();
    }

    ImGui::Separator();

    // Scene Queue Table
    ImGui::Text("Scene Queue:");
    if (ImGui::BeginTable("QueueTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Order");
        ImGui::TableSetupColumn("Scene Name");
        ImGui::TableSetupColumn("Status");
        ImGui::TableHeadersRow();

        std::queue<std::string> tempQueue = sceneQueue_;
        int order = 1;
        while (!tempQueue.empty()) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%d", order++);
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", tempQueue.front().c_str());
            ImGui::TableSetColumnIndex(2);
            auto it = scenes_.find(tempQueue.front());
            if (it != scenes_.end()) {
                std::string statusText = PrintSceneStatus(it->second.status);
                ImGui::Text("%s", statusText.c_str());
            }
            tempQueue.pop();
        }
        ImGui::EndTable();
    }

    ImGui::End();
}

void SceneService::ToggleVisibility() {
    inspectorVisible_ = !inspectorVisible_;
}

float SceneService::GetTransitionProgress() const {
    if (!IsTransitioning() || transitionDuration_ == 0.0f) {
        return 0.0f;
    }
    return transitionTimer_ / transitionDuration_;
}

void SceneService::StartTimeout() {
    // Only allow timeout when paused and not already timing out
    if (transitionStateMachine_.IsInState("ACTIVE_PAUSED") && !isTimingOut_) {
        isTimingOut_ = true;
        timeoutTimer_ = 0.0f;
        transitionStateMachine_.TransitionTo("ACTIVE_RUNNING");
    }
}

void SceneService::OnAssetsLoaded() {
    if (transitionStateMachine_.IsInState("LOADING_ASSETS")) {
        transitionStateMachine_.TransitionTo("ASSETS_LOADED");
    }
}

void SceneService::Shutdown() {
    if (activeScene_) {
        activeScene_->OnExit();
        activeScene_ = nullptr;
    }

    while (!sceneQueue_.empty()) {
        sceneQueue_.pop();
    }

    for (auto& [name, data] : scenes_) {
        delete data.scene;
    }

    scenes_.clear();
    pendingAssets_.clear();
    transitionTimer_ = 0.0f;
}

} // namespace Elysium::Services
