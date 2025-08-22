#pragma once

#include <unordered_map>
#include <memory>
#include <string>
#include <vector>
#include <queue>
#include "Scene.h"
#include "Asset.h"
#include "StateMachine.h"

namespace Elysium::Services {
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


class SceneService {
public:

    // The SceneService manages scene transitions through the SceneStatus FSM.
    // During LOADING_ASSETS, it waits for LoadingService to complete before proceeding.
    // It will be notified with OnAssetsLoaded() to transition from LOADING_ASSETS to ASSETS_LOADED.

    SceneService();
    ~SceneService() = default;
    SceneService(const SceneService&) = delete;
    SceneService& operator=(const SceneService&) = delete;

    // Scene management
    void RegisterScene(const std::string& name, std::string xmlPath, SceneFactory factory);
    Scene* GetScene() const;
    void SetScene(std::string name);
    void QueueScene(std::string name);

    // Scene lifecycle methods
    void Update(float deltaTime);
    void DebugDraw();
    bool IsTransitioning() const; 
    const std::string& GetCurrentState() const { return transitionStateMachine_.GetCurrentState(); }
    float GetTransitionProgress() const;
    
    // Debug timeout
    void StartTimeout();
    void SetTimeoutMs(float milliseconds) { timeoutDuration_ = milliseconds; }

    // Asset management for transitions
    const std::vector<Asset>& GetPendingAssets() const { return pendingAssets_; }
    void OnAssetsLoaded(); // Called when asset loading completes

    // Cleanup
    void Shutdown();

    void ToggleVisibility();
    bool IsVisible() const { return inspectorVisible_; }


private:
    // State machine handlers
    void InitializeStateMachine();
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

    bool inspectorVisible_ = false;

    // Queue-based scene management
    std::queue<std::string> sceneQueue_;
    Scene* activeScene_ = nullptr;
    std::vector<Asset> pendingAssets_;

    // State machine for transitions
    StateMachine transitionStateMachine_;
    
    // Transition timing
    float transitionTimer_ = 0.0f;
    float transitionDuration_ = 1.0f;
    
    // Pause/unpause deltaTime handling
    float cachedDeltaTime_ = 0.016f; // Default 60fps
    
    // Timeout system
    float timeoutDuration_ = 100.0f; // Timeout duration in milliseconds
    float timeoutTimer_ = 0.0f; // Current timeout timer
    bool isTimingOut_ = false; // Flag for timeout mode
};

} // namespace Elysium::Services
