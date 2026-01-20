#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Elysium {

class StateMachine {
   public:
    using StateHandler = std::function<void()>;
    using TransitionHandler = std::function<void(const std::string& from, const std::string& to)>;

    StateMachine() = default;
    ~StateMachine() = default;

    // State management
    void SetCurrentState(const std::string& state);
    const std::string& GetCurrentState() const { return currentState_; }
    bool IsInState(const std::string& state) const { return currentState_ == state; }

    // State handlers
    void RegisterOnEnter(const std::string& state, StateHandler handler);
    void RegisterOnUpdate(const std::string& state, StateHandler handler);
    void RegisterOnExit(const std::string& state, StateHandler handler);

    // Transition handlers
    void RegisterOnTransition(const std::string& from, const std::string& to, TransitionHandler handler);

    // Valid transitions
    void AddValidTransition(const std::string& from, const std::string& to);
    bool CanTransition(const std::string& from, const std::string& to) const;

    // State machine operations
    bool TransitionTo(const std::string& newState);
    void Update();

    // Debugging
    std::vector<std::string> GetValidTransitionsFrom(const std::string& state) const;
    bool IsValidState(const std::string& state) const;

   private:
    std::string currentState_;
    std::string previousState_;

    // State handlers
    std::unordered_map<std::string, StateHandler> onEnterHandlers_;
    std::unordered_map<std::string, StateHandler> onUpdateHandlers_;
    std::unordered_map<std::string, StateHandler> onExitHandlers_;

    // Transition handlers
    std::unordered_map<std::string, TransitionHandler> transitionHandlers_;

    // Valid transitions (from -> to)
    std::unordered_map<std::string, std::vector<std::string>> validTransitions_;

    std::string MakeTransitionKey(const std::string& from, const std::string& to) const;
};

}  // namespace Elysium