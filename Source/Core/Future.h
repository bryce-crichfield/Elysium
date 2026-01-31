#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

namespace Elysium {

template <typename T>
class Future  {
    struct SharedState {
        std::optional<T> value;
        std::vector<std::function<void(const T&)>> continuations;
        bool resolved = false;
    };

    std::shared_ptr<SharedState> state_;

public:
    Future() : state_(std::make_shared<SharedState>()) {}

    bool IsReady() const { return state_->resolved; }

    const T& Get() const {
        if (!state_->resolved) {
            throw std::runtime_error("Future::Get called before resolution");
        }
        return *state_->value;
    }

    Future<T>& Then(std::function<void(const T&)> fn) {
        if (state_->resolved) {
            fn(*state_->value);
        } else {
            state_->continuations.push_back(std::move(fn));
        }
        return *this;
    }

    void Resolve(T value) {
        if (state_->resolved) {
            throw std::runtime_error("Future::Resolve called on already-resolved future");
        }
        state_->value = std::move(value);
        state_->resolved = true;
        for (auto& fn : state_->continuations) {
            fn(*state_->value);
        }
        state_->continuations.clear();
    }
};

}  // namespace Elysium
