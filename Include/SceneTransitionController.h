#pragma once

#include "StateMachine.h"
#include <functional>

namespace Elysium {

namespace Services {
    enum class SceneState;
    class SceneService;
}

class SceneTransitionController {
public:
    friend class Services::SceneService;
    friend class SceneInspector;

    SceneTransitionController() = default;
    ~SceneTransitionController() = default;

    void Initialize();
    void Update(float deltaTime);

    // State queries
    Services::SceneState GetCurrentState() const;
    bool IsTransitioning() const;
    float GetTransitionProgress() const;

    // State transitions
    void TransitionToExiting();
    void OnAssetsLoaded();  // LOADING_ASSETS -> ASSETS_LOADED

    // Callbacks set by SceneService for state change handling
    std::function<void()> onEnterExiting;
    std::function<void()> onUpdateExiting;
    std::function<void()> onEnterLoadingAssets;
    std::function<void()> onEnterAssetsLoaded;
    std::function<void()> onEnterEntering;
    std::function<void()> onUpdateEntering;
    std::function<void()> onEnterActive;
    std::function<void()> onEnterActiveRunning;
    std::function<void()> onEnterActivePaused;

    // Timeout system for debugging
    void StartTimeout();
    void SetTimeoutDuration(float milliseconds) { timeoutDuration_ = milliseconds; }

    // Access to transition timing (for adjusting fade durations)
    void SetTransitionDuration(float duration) { transitionDuration_ = duration; }
    float GetTransitionDuration() const { return transitionDuration_; }

private:
    void InitializeStateMachine();

    StateMachine stateMachine_;

    // Transition timing
    float transitionTimer_ = 0.0f;
    float transitionDuration_ = 1.0f;

    // Timeout system
    float timeoutDuration_ = 100.0f;  // milliseconds
    float timeoutTimer_ = 0.0f;
    bool isTimingOut_ = false;
};

} // namespace Elysium
