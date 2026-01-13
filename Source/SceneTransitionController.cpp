#include "SceneTransitionController.h"
#include "Services/SceneService.h"
#include "Common.h"
#include "raylib.h"

namespace Elysium {

using namespace Services;

void SceneController::Initialize()
{
    Profile;
    InitializeStateMachine();
}

void SceneController::InitializeStateMachine()
{
    // Scene transition states
    const auto NONE = SceneStateToString(SceneState::NONE);
    const auto EXITING = SceneStateToString(SceneState::EXITING);
    const auto LOADING_ASSETS = SceneStateToString(SceneState::LOADING_ASSETS);
    const auto ASSETS_LOADED = SceneStateToString(SceneState::ASSETS_LOADED);
    const auto ENTERING = SceneStateToString(SceneState::ENTERING);
    const auto ACTIVE = SceneStateToString(SceneState::ACTIVE);
    const auto ACTIVE_RUNNING = SceneStateToString(SceneState::ACTIVE_RUNNING);
    const auto ACTIVE_PAUSED = SceneStateToString(SceneState::ACTIVE_PAUSED);

    // Valid state transitions: NONE/ACTIVE_* -> EXITING -> LOADING_ASSETS -> ASSETS_LOADED -> ENTERING -> ACTIVE ->
    // ACTIVE_RUNNING <-> ACTIVE_PAUSED
    stateMachine_.AddValidTransition(NONE, EXITING);
    stateMachine_.AddValidTransition(NONE, LOADING_ASSETS);
    stateMachine_.AddValidTransition(ACTIVE_RUNNING, EXITING);
    stateMachine_.AddValidTransition(ACTIVE_PAUSED, EXITING);
    stateMachine_.AddValidTransition(EXITING, LOADING_ASSETS);
    stateMachine_.AddValidTransition(LOADING_ASSETS, ASSETS_LOADED);
    stateMachine_.AddValidTransition(ASSETS_LOADED, ENTERING);
    stateMachine_.AddValidTransition(ENTERING, ACTIVE);
    stateMachine_.AddValidTransition(ACTIVE, ACTIVE_RUNNING);
    stateMachine_.AddValidTransition(ACTIVE_RUNNING, ACTIVE_PAUSED);
    stateMachine_.AddValidTransition(ACTIVE_PAUSED, ACTIVE_RUNNING);

    // State event handlers
    stateMachine_.RegisterOnEnter(EXITING, [this]() {
        transitionTimer_ = 0.0f;
        if (onEnterExiting) onEnterExiting();
    });

    stateMachine_.RegisterOnEnter(LOADING_ASSETS, [this]() {
        if (onEnterLoadingAssets) onEnterLoadingAssets();
    });

    stateMachine_.RegisterOnEnter(ASSETS_LOADED, [this]() {
        if (onEnterAssetsLoaded) onEnterAssetsLoaded();
    });

    stateMachine_.RegisterOnEnter(ENTERING, [this]() {
        transitionTimer_ = 0.0f;
        if (onEnterEntering) onEnterEntering();
    });

    stateMachine_.RegisterOnEnter(ACTIVE, [this]() {
        if (onEnterActive) onEnterActive();
    });

    stateMachine_.RegisterOnEnter(ACTIVE_RUNNING, [this]() {
        if (onEnterActiveRunning) onEnterActiveRunning();
    });

    stateMachine_.RegisterOnEnter(ACTIVE_PAUSED, [this]() {
        if (onEnterActivePaused) onEnterActivePaused();
    });

    // Set initial state
    stateMachine_.SetCurrentState(NONE);
}

void SceneController::Update(float deltaTime)
{
    Profile;

    // Update state machine
    stateMachine_.Update();

    // Handle state-specific update logic
    SceneState currentState = GetCurrentState();

    if (currentState == SceneState::EXITING)
    {
        transitionTimer_ += deltaTime;
        if (transitionTimer_ >= transitionDuration_)
        {
            stateMachine_.TransitionTo(SceneStateToString(SceneState::LOADING_ASSETS));
        }
        if (onUpdateExiting) onUpdateExiting();
    }
    else if (currentState == SceneState::ENTERING)
    {
        transitionTimer_ += deltaTime;
        if (transitionTimer_ >= transitionDuration_)
        {
            stateMachine_.TransitionTo(SceneStateToString(SceneState::ACTIVE));
        }
        if (onUpdateEntering) onUpdateEntering();
    }

    // Handle timeout system
    if (isTimingOut_)
    {
        timeoutTimer_ += deltaTime * 1000.0f;  // Convert to milliseconds

        if (timeoutTimer_ >= timeoutDuration_)
        {
            isTimingOut_ = false;
            timeoutTimer_ = 0.0f;
            stateMachine_.TransitionTo(SceneStateToString(SceneState::ACTIVE_PAUSED));
        }
    }
}

SceneState SceneController::GetCurrentState() const
{
    return StringToSceneState(stateMachine_.GetCurrentState());
}

bool SceneController::IsTransitioning() const
{
    SceneState state = GetCurrentState();
    return state == SceneState::EXITING ||
           state == SceneState::LOADING_ASSETS ||
           state == SceneState::ASSETS_LOADED ||
           state == SceneState::ENTERING;
}

float SceneController::GetTransitionProgress() const
{
    if (transitionDuration_ <= 0.0f)
        return 1.0f;
    return transitionTimer_ / transitionDuration_;
}

void SceneController::TransitionToExiting()
{
    stateMachine_.TransitionTo(SceneStateToString(SceneState::EXITING));
}

void SceneController::OnAssetsLoaded()
{
    if (stateMachine_.IsInState(SceneStateToString(SceneState::LOADING_ASSETS)))
    {
        stateMachine_.TransitionTo(SceneStateToString(SceneState::ASSETS_LOADED));
    }
}

void SceneController::StartTimeout()
{
    if (stateMachine_.IsInState(SceneStateToString(SceneState::ACTIVE_PAUSED)) && !isTimingOut_)
    {
        isTimingOut_ = true;
        timeoutTimer_ = 0.0f;
        stateMachine_.TransitionTo(SceneStateToString(SceneState::ACTIVE_RUNNING));
    }
}

} // namespace Elysium
