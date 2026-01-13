#pragma once

#include <unordered_map>
#include <memory>
#include <string>
#include <vector>
#include <queue>
#include "Scene.h"
#include "Asset.h"
#include "Service.h"
#include "SceneRenderer.h"
#include "SceneTransitionController.h"
#include "SceneInspector.h"

namespace Elysium::Services {
    enum class SceneState {
        NONE,
        EXITING,
        LOADING_ASSETS,
        ASSETS_LOADED,
        ENTERING,
        ACTIVE,
        ACTIVE_RUNNING,
        ACTIVE_PAUSED
    };

    // Convert between SceneState enum and string (for StateMachine)
    const char* SceneStateToString(SceneState state);
    SceneState StringToSceneState(const std::string& str);

    enum class SceneStatus {
        UNLOADED,           // Scene registered but no assets loaded, no instance created
        LOADING_ASSETS,     // Assets are being loaded by LoadingService
        ASSETS_LOADED,      // Assets ready, but scene instance not created yet
        INITIALIZING,       // Scene instance being created and initialized
        ENTERING,           // Scene transitioning in (fade in, entrance animations)
        ACTIVE,             // Scene running normally, receiving updates
        EXITING,            // Scene transitioning out (fade out, exit animations)
        SUSPENDED           // Scene paused/inactive but still in memory
    };

    std::string PrintSceneStatus(SceneStatus status);

    struct SceneData {
        std::string name;

        Scene* scene;
        SceneFactory factory;
        std::vector<std::string> assets;
        std::string xmlPath;
        bool xmlLoaded = false;

        SceneStatus status;
    };


class SceneService : public Elysium::Service {
public:
    friend class Elysium::SceneInspector;

    // The SceneService manages scene transitions through the SceneStatus FSM.
    // During LOADING_ASSETS, it waits for LoadingService to complete before proceeding.
    // It will be notified with OnAssetsLoaded() to transition from LOADING_ASSETS to ASSETS_LOADED.

    SceneService();
    ~SceneService() = default;
    SceneService(const SceneService&) = delete;
    SceneService& operator=(const SceneService&) = delete;

    // Service interface
    void Initialize() override;
    void Shutdown() override;
    void Update(float deltaTime) override;
    void Render() override;
    void ImGui() override;

    // Scene management
    void RegisterScene(const std::string& name, std::string xmlPath, SceneFactory factory);
    Elysium::Scene* GetScene() const;
    void SetScene(std::string name);
    void QueueScene(std::string name);

    // Scene lifecycle methods
    bool IsTransitioning() const { return transitions_.IsTransitioning(); }
    SceneState GetCurrentState() const { return transitions_.GetCurrentState(); }
    float GetTransitionProgress() const { return transitions_.GetTransitionProgress(); }

    // Asset management for transitions
    const std::vector<Asset>& GetPendingAssets() const { return pendingAssets_; }
    void OnAssetsLoaded(); // Called when asset loading completes

private:
    // Components
    Elysium::SceneRenderer renderer_;
    Elysium::SceneTransitionController transitions_;
    Elysium::SceneInspector inspector_;

    // State machine handlers (callbacks for transition controller)
    void OnEnterExiting();
    void OnUpdateExiting();
    void OnEnterLoadingAssets();
    void OnEnterAssetsLoaded();
    void OnEnterEntering();
    void OnUpdateEntering();
    void OnEnterActive();
    void OnEnterActiveRunning();
    void OnEnterActivePaused();

    Scene* CreateOrGetScene(const std::string& name);
    void EnterScene(const std::string& name);
    void UpdateSceneStatus(const std::string& name, SceneStatus status);
    void UpdateActiveSceneStatus(SceneStatus status);

    std::unordered_map<std::string, SceneData> scenes_;

    // Queue-based scene management
    std::queue<std::string> sceneQueue_;
    Scene* activeScene_ = nullptr;
    std::vector<Asset> pendingAssets_;

    // Pause/unpause deltaTime handling
    float cachedDeltaTime_ = 0.016f; // Default 60fps
};

} // namespace Elysium::Services
