#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <vector>

namespace Elysium {

template <typename T>
class Future {
    struct SharedState {
        std::mutex mutex;
        std::optional<T> value;
        std::vector<std::function<void(const T&)>> continuations;
        bool resolved = false;
    };

    std::shared_ptr<SharedState> state_;

public:
    Future() : state_(std::make_shared<SharedState>()) {}

    bool IsReady() const {
        std::lock_guard<std::mutex> lock(state_->mutex);
        return state_->resolved;
    }

    const T& Get() const {
        std::lock_guard<std::mutex> lock(state_->mutex);
        if (!state_->resolved) {
            throw std::runtime_error("Future::Get called before resolution");
        }
        return *state_->value;
    }

    // Register a continuation to run on the main thread when Poll() is called
    Future<T>& Then(std::function<void(const T&)> fn) {
        std::lock_guard<std::mutex> lock(state_->mutex);
        state_->continuations.push_back(std::move(fn));
        return *this;
    }

    // Called from worker thread — stores the value but does NOT fire continuations
    void Resolve(T value) {
        std::lock_guard<std::mutex> lock(state_->mutex);
        if (state_->resolved) {
            throw std::runtime_error("Future::Resolve called on already-resolved future");
        }
        state_->value = std::move(value);
        state_->resolved = true;
    }

    // Called from main thread — fires pending continuations if resolved
    // Returns true if the future is resolved
    bool Poll() {
        std::vector<std::function<void(const T&)>> toFire;
        const T* valuePtr = nullptr;

        {
            std::lock_guard<std::mutex> lock(state_->mutex);
            if (!state_->resolved) {
                return false;
            }
            if (!state_->continuations.empty()) {
                toFire = std::move(state_->continuations);
                state_->continuations.clear();
            }
            valuePtr = &(*state_->value);
        }

        // Fire continuations without holding the lock
        for (auto& fn : toFire) {
            fn(*valuePtr);
        }
        return true;
    }
};

}  // namespace Elysium
