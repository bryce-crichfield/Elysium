#include "StateMachine.h"
#include "Services/LogService.h"
#include <algorithm>

namespace Elysium {

void StateMachine::SetCurrentState(const std::string& state) {
    if (!currentState_.empty()) {
        // Call exit handler for current state
        auto exitIt = onExitHandlers_.find(currentState_);
        if (exitIt != onExitHandlers_.end()) {
            exitIt->second();
        }
    }

    previousState_ = currentState_;
    currentState_ = state;

    // Call enter handler for new state
    auto enterIt = onEnterHandlers_.find(currentState_);
    if (enterIt != onEnterHandlers_.end()) {
        enterIt->second();
    }

    LOG_DEBUGF("StateMachine", "State changed: %s -> %s", 
               previousState_.empty() ? "NONE" : previousState_.c_str(), 
               currentState_.c_str());
}

void StateMachine::RegisterOnEnter(const std::string& state, StateHandler handler) {
    onEnterHandlers_[state] = handler;
}

void StateMachine::RegisterOnUpdate(const std::string& state, StateHandler handler) {
    onUpdateHandlers_[state] = handler;
}

void StateMachine::RegisterOnExit(const std::string& state, StateHandler handler) {
    onExitHandlers_[state] = handler;
}

void StateMachine::RegisterOnTransition(const std::string& from, const std::string& to, TransitionHandler handler) {
    std::string key = MakeTransitionKey(from, to);
    transitionHandlers_[key] = handler;
}

void StateMachine::AddValidTransition(const std::string& from, const std::string& to) {
    validTransitions_[from].push_back(to);
}

bool StateMachine::CanTransition(const std::string& from, const std::string& to) const {
    auto it = validTransitions_.find(from);
    if (it == validTransitions_.end()) {
        return false;
    }

    const auto& transitions = it->second;
    return std::find(transitions.begin(), transitions.end(), to) != transitions.end();
}

bool StateMachine::TransitionTo(const std::string& newState) {
    if (currentState_ == newState) {
        return true; // Already in target state
    }

    if (!currentState_.empty() && !CanTransition(currentState_, newState)) {
        LOG_ERRORF("StateMachine", "Invalid transition: %s -> %s", currentState_.c_str(), newState.c_str());
        return false;
    }

    // Call transition handler
    std::string transitionKey = MakeTransitionKey(currentState_, newState);
    auto transitionIt = transitionHandlers_.find(transitionKey);
    if (transitionIt != transitionHandlers_.end()) {
        transitionIt->second(currentState_, newState);
    }

    SetCurrentState(newState);
    return true;
}

void StateMachine::Update() {
    if (currentState_.empty()) {
        return;
    }

    auto updateIt = onUpdateHandlers_.find(currentState_);
    if (updateIt != onUpdateHandlers_.end()) {
        updateIt->second();
    }
}

std::vector<std::string> StateMachine::GetValidTransitionsFrom(const std::string& state) const {
    auto it = validTransitions_.find(state);
    if (it != validTransitions_.end()) {
        return it->second;
    }
    return {};
}

bool StateMachine::IsValidState(const std::string& state) const {
    return onEnterHandlers_.find(state) != onEnterHandlers_.end() ||
           onUpdateHandlers_.find(state) != onUpdateHandlers_.end() ||
           onExitHandlers_.find(state) != onExitHandlers_.end();
}

std::string StateMachine::MakeTransitionKey(const std::string& from, const std::string& to) const {
    return from + "->" + to;
}

} // namespace Elysium