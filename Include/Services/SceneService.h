#pragma once

#include <unordered_map>
#include <memory>
#include <string>
#include <vector>
#include "Scene.h"
#include "Asset.h"

namespace Elysium::Services {
class SceneService {
public:
    enum class TransitionState {
        NONE,
        FADE_OUT,
        LOADING,
        FADE_IN
    };

    SceneService() = default;
    ~SceneService() = default;
    SceneService(const SceneService&) = delete;
    SceneService& operator=(const SceneService&) = delete;

    // Scene management
    Scene* GetScene() const { return currentScene_.get(); }
    void SetScene(std::unique_ptr<Scene> scene);

    void QueueScene(const std::string& xmlPath);
    void QueueScene(std::unique_ptr<Scene> scene);

    // Scene factory registration and XML loading
    void RegisterScene(const std::string& typeName, SceneFactory factory);

    // Scene lifecycle methods
    void Update(float deltaTime);
    void ProcessEvents();
    bool IsTransitioning() const { return transitionState_ != TransitionState::NONE; }
    TransitionState GetTransitionState() const { return transitionState_; }
    float GetTransitionProgress() const;

    // Transition configuration
    float GetTransitionDuration() const { return transitionDuration_; }
    void SetTransitionDuration(float duration) { transitionDuration_ = duration; }

    // Asset management for transitions
    const std::vector<Asset>& GetPendingAssets() const { return pendingAssets_; }
    void ClearPendingAssets() { pendingAssets_.clear(); }
    void OnAssetsLoaded(); // Called when asset loading completes

    // Cleanup
    void Shutdown();

private:
    void HandleSceneTransition();

    std::unique_ptr<Scene> currentScene_;
    std::unique_ptr<Scene> pendingScene_;
    bool sceneTransitionPending_ = false;
    bool sceneTransitionLocked_ = false;

    // Scene factory registry
    std::unordered_map<std::string, SceneFactory> sceneFactories_;

    TransitionState transitionState_ = TransitionState::NONE;
    float transitionTimer_ = 0.0f;
    float transitionDuration_ = 1.0f;
    std::vector<Asset> pendingAssets_;
};

} // namespace Elysium::Services
