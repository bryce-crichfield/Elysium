#include "Services/SceneService.h"
#include "Services/LogService.h"
#include "Application.h"
#include "System.h"
#include "Path.h"
#include "imgui.h"
#include "raylib.h"
#include "tinyxml2.h"
#include <typeinfo>
#include "Common.h"

using namespace tinyxml2;

namespace Elysium::Services
{

// =============================================================================
// SceneState Conversion
// =============================================================================

const char* SceneStateToString(SceneState state)
{
    switch (state)
    {
        case SceneState::NONE: return "NONE";
        case SceneState::EXITING: return "EXITING";
        case SceneState::LOADING_ASSETS: return "LOADING_ASSETS";
        case SceneState::ASSETS_LOADED: return "ASSETS_LOADED";
        case SceneState::ENTERING: return "ENTERING";
        case SceneState::ACTIVE: return "ACTIVE";
        case SceneState::ACTIVE_RUNNING: return "ACTIVE_RUNNING";
        case SceneState::ACTIVE_PAUSED: return "ACTIVE_PAUSED";
        default: return "NONE";
    }
}

SceneState StringToSceneState(const std::string& str)
{
    if (str == "NONE") return SceneState::NONE;
    if (str == "EXITING") return SceneState::EXITING;
    if (str == "LOADING_ASSETS") return SceneState::LOADING_ASSETS;
    if (str == "ASSETS_LOADED") return SceneState::ASSETS_LOADED;
    if (str == "ENTERING") return SceneState::ENTERING;
    if (str == "ACTIVE") return SceneState::ACTIVE;
    if (str == SceneStateToString(SceneState::ACTIVE_RUNNING)) return SceneState::ACTIVE_RUNNING;
    if (str == SceneStateToString(SceneState::ACTIVE_PAUSED)) return SceneState::ACTIVE_PAUSED;
    return SceneState::NONE;
}

// =============================================================================
// Constructor & State Machine Setup
// =============================================================================

SceneService::SceneService()
{
    name_ = "SceneService";
}

void SceneService::Initialize()
{
    Profile;
    auto& app = Application::GetInstance();
    const auto& config = app.GetConfig();

    // Initialize components
    renderer_.Initialize(config.framebufferWidth, config.framebufferHeight);
    transitions_.Initialize();

    // Wire up state machine callbacks
    transitions_.onEnterExiting = [this]() { OnEnterExiting(); };
    transitions_.onUpdateExiting = [this]() { OnUpdateExiting(); };
    transitions_.onEnterLoadingAssets = [this]() { OnEnterLoadingAssets(); };
    transitions_.onEnterAssetsLoaded = [this]() { OnEnterAssetsLoaded(); };
    transitions_.onEnterEntering = [this]() { OnEnterEntering(); };
    transitions_.onUpdateEntering = [this]() { OnUpdateEntering(); };
    transitions_.onEnterActive = [this]() { OnEnterActive(); };
    transitions_.onEnterActiveRunning = [this]() { OnEnterActiveRunning(); };
    transitions_.onEnterActivePaused = [this]() { OnEnterActivePaused(); };
}

// =============================================================================
// State Machine Event Handlers
// =============================================================================

void SceneService::OnEnterExiting()
{
    Profile;
    transitions_.SetTransitionDuration((sceneQueue_.size() > 1) ? 0.1f : 1.0f);
}

void SceneService::OnUpdateExiting()
{
    Profile;
    // Timer logic and automatic transition handled by SceneTransitionController
}

void SceneService::OnEnterLoadingAssets()
{
    Profile;

    if (!sceneQueue_.empty())
    {
        const std::string &sceneName = sceneQueue_.front();
        UpdateSceneStatus(sceneName, SceneStatus::LOADING_ASSETS);

        Scene *scene = CreateOrGetScene(sceneName);
        if (scene)
        {
            pendingAssets_ = scene->GetAssets();
            LOG_INFOF("SceneService", "Scene '%s' requested %zu assets for loading", sceneName.c_str(),
                      pendingAssets_.size());
            for (const auto &asset : pendingAssets_)
            {
                LOG_DEBUGF("SceneService", "Asset requested: %s (%s) -> %s", asset.GetName().c_str(),
                           asset.GetType() == AssetType::TEXTURE  ? "TEXTURE"
                           : asset.GetType() == AssetType::SOUND  ? "SOUND"
                           : asset.GetType() == AssetType::MUSIC  ? "MUSIC"
                           : asset.GetType() == AssetType::FONT   ? "FONT"
                           : asset.GetType() == AssetType::MODEL  ? "MODEL"
                           : asset.GetType() == AssetType::SHADER ? "SHADER"
                           : asset.GetType() == AssetType::SPRITE ? "SPRITE"
                                                                  : "UNKNOWN",
                           asset.GetPath().c_str());
            }
        }
        else
        {
            LOG_ERRORF("SceneService", "Failed to create/get scene '%s' for asset loading", sceneName.c_str());
        }
    }
}

void SceneService::OnEnterAssetsLoaded()
{
    Profile;
    if (!sceneQueue_.empty())
    {
        UpdateSceneStatus(sceneQueue_.front(), SceneStatus::ASSETS_LOADED);
    }
    transitions_.stateMachine_.TransitionTo(SceneStateToString(SceneState::ENTERING));
}

void SceneService::OnEnterEntering()
{
    Profile;
    if (!sceneQueue_.empty())
    {
        UpdateSceneStatus(sceneQueue_.front(), SceneStatus::ENTERING);
    }
}

void SceneService::OnUpdateEntering()
{
    Profile;
    // Timer logic and automatic transition handled by SceneTransitionController
}

void SceneService::OnEnterActive()
{
    Profile;
    if (!sceneQueue_.empty())
    {
        const std::string sceneName = sceneQueue_.front();
        sceneQueue_.pop();

        // Cleanup old scene
        if (activeScene_)
        {
            activeScene_->OnExit();
            UpdateActiveSceneStatus(SceneStatus::SUSPENDED);
        }

        // Activate new scene
        activeScene_ = CreateOrGetScene(sceneName);
        if (activeScene_)
        {
            EnterScene(sceneName);
            UpdateSceneStatus(sceneName, SceneStatus::ACTIVE);
        }
    }

    // Automatically transition to ACTIVE_RUNNING
    transitions_.stateMachine_.TransitionTo(SceneStateToString(SceneState::ACTIVE_RUNNING));
}

void SceneService::OnEnterActiveRunning()
{
    // Scene is now running - updates will be called
}

void SceneService::OnEnterActivePaused()
{
    // Scene is paused - updates will not be called, but rendering continues
}

// =============================================================================
// Helper Methods
// =============================================================================

void SceneService::UpdateSceneStatus(const std::string &name, SceneStatus status)
{
    Profile;
    auto it = scenes_.find(name);
    if (it != scenes_.end())
    {
        it->second.status = status;
    }
}

void SceneService::UpdateActiveSceneStatus(SceneStatus status)
{
    for (auto &[name, data] : scenes_)
    {
        if (data.scene == activeScene_)
        {
            data.status = status;
            break;
        }
    }
}

std::string PrintSceneStatus(SceneStatus status)
{
    switch (status)
    {
    case SceneStatus::UNLOADED:
        return "UNLOADED";
    case SceneStatus::LOADING_ASSETS:
        return "LOADING_ASSETS";
    case SceneStatus::ASSETS_LOADED:
        return "ASSETS_LOADED";
    case SceneStatus::INITIALIZING:
        return "INITIALIZING";
    case SceneStatus::ENTERING:
        return "ENTERING";
    case SceneStatus::ACTIVE:
        return "ACTIVE";
    case SceneStatus::EXITING:
        return "EXITING";
    case SceneStatus::SUSPENDED:
        return "SUSPENDED";
    }
    return "UNKNOWN";
}

// =============================================================================
// Public Interface
// =============================================================================

Elysium::Scene *SceneService::GetScene() const
{
    return activeScene_;
}

void SceneService::SetScene(std::string name)
{
    // Clear queue and force immediate scene transition
    while (!sceneQueue_.empty())
    {
        sceneQueue_.pop();
    }

    sceneQueue_.push(name);
    transitions_.SetTransitionDuration(0.0f); // Immediate transition

    if (activeScene_)
    {
        transitions_.stateMachine_.TransitionTo(SceneStateToString(SceneState::EXITING));
    }
    else
    {
        transitions_.stateMachine_.TransitionTo(SceneStateToString(SceneState::LOADING_ASSETS));
    }
}

void SceneService::RegisterScene(const std::string &name, std::string xmlPath, SceneFactory factory)
{
    SceneData data{name, nullptr, factory, {}, xmlPath, false, SceneStatus::UNLOADED};
    scenes_.emplace(name, data);
    LOG_INFOF("SceneService", "Registered scene: %s", name.c_str());
}

void SceneService::QueueScene(std::string name)
{
    auto it = scenes_.find(name);
    if (it == scenes_.end())
    {
        LOG_ERRORF("SceneService", "Scene not found: %s", name.c_str());
        return;
    }

    sceneQueue_.push(name);
    LOG_INFOF("SceneService", "Queued scene: %s", name.c_str());
}

void SceneService::Update(float deltaTime)
{
    Profile;
    // Cache deltaTime for smooth pause/unpause transitions
    if (deltaTime > 0.0f && deltaTime < 0.1f)
    { // Reasonable deltaTime bounds
        cachedDeltaTime_ = deltaTime;
    }

    SceneState currentState = transitions_.GetCurrentState();

    // Start transition if we have queued scenes and can transition
    if (!sceneQueue_.empty() &&
        (currentState == SceneState::NONE || currentState == SceneState::ACTIVE_RUNNING || currentState == SceneState::ACTIVE_PAUSED))
    {
        if (activeScene_ && (currentState == SceneState::ACTIVE_RUNNING || currentState == SceneState::ACTIVE_PAUSED))
        {
            transitions_.stateMachine_.TransitionTo(SceneStateToString(SceneState::EXITING));
        }
        else if (!activeScene_ && currentState == SceneState::NONE)
        {
            transitions_.stateMachine_.TransitionTo(SceneStateToString(SceneState::LOADING_ASSETS));
        }
    }

    // Update transition controller (handles state machine and timing)
    transitions_.Update(deltaTime);

    // Update active scene when running
    if (activeScene_ && currentState == SceneState::ACTIVE_RUNNING)
    {
        activeScene_->OnUpdate(cachedDeltaTime_);
    }
}

// =============================================================================
// Scene Management
// =============================================================================

Scene *SceneService::CreateOrGetScene(const std::string &name)
{
    auto it = scenes_.find(name);
    if (it == scenes_.end())
    {
        LOG_ERRORF("SceneService", "Scene not found: %s", name.c_str());
        return nullptr;
    }

    SceneData &sceneData = it->second;
    if (!sceneData.scene)
    {
        sceneData.scene = sceneData.factory();
    }

    return sceneData.scene;
}

void SceneService::EnterScene(const std::string &name)
{
    auto it = scenes_.find(name);
    if (it == scenes_.end())
    {
        LOG_ERRORF("SceneService", "Cannot enter scene. Scene not found: %s", name.c_str());
        return;
    }

    SceneData &sceneData = it->second;
    Scene *scene = CreateOrGetScene(name);

    if (!scene)
    {
        LOG_ERRORF("SceneService", "Cannot enter scene. Scene not found: %s", name.c_str());
        return;
    }

    sceneData.status = SceneStatus::INITIALIZING;
    if (!sceneData.xmlPath.empty() && !sceneData.xmlLoaded)
    {
        LoadScene(*sceneData.scene, Path(sceneData.xmlPath).GetFullPath());
        sceneData.xmlLoaded = true;
    }
    sceneData.status = SceneStatus::ENTERING;
    scene->OnEnter();
}

// =============================================================================
// Rendering
// =============================================================================

void SceneService::Render()
{
    Profile;
    auto& app = Application::GetInstance();
    const auto& config = app.GetConfig();

    renderer_.Render(
        GetScene(),
        transitions_.GetCurrentState(),
        transitions_.GetTransitionProgress(),
        transitions_.IsTransitioning(),
        config.backgroundColor
    );
}

// =============================================================================
// Debug & Utility
// =============================================================================

void SceneService::ImGui()
{
    Profile;
    inspector_.DrawUI(*this);
}

void SceneService::OnAssetsLoaded()
{
    transitions_.OnAssetsLoaded();
}

void SceneService::Shutdown()
{
    Profile;
    renderer_.Shutdown();

    if (activeScene_)
    {
        activeScene_->OnExit();
        activeScene_ = nullptr;
    }

    while (!sceneQueue_.empty())
    {
        sceneQueue_.pop();
    }

    for (auto &[name, data] : scenes_)
    {
        delete data.scene;
    }

    scenes_.clear();
    pendingAssets_.clear();
}

} // namespace Elysium::Services
