#pragma once

#include <functional>
#include <memory>
#include <typeindex>
#include "Core/Message.h"

namespace Elysium::Services {

// Post<T>/Subscribe<T> can't be virtual (templates), so the interface exposes
// type-erased primitives and keeps the templated convenience API inline here,
// calling straight down to them — callers keep the same ergonomic API.
class IMessageService {
   public:
    virtual ~IMessageService() = default;

    virtual void PostRaw(std::unique_ptr<Elysium::Message> message) = 0;
    virtual void SubscribeRaw(std::type_index type, void* owner,
                              std::function<void(const Elysium::Message&)> handler) = 0;
    virtual void UnsubscribeAll(void* owner) = 0;

    template <typename T, typename... Args>
    void Post(Args&&... args) {
        PostRaw(std::make_unique<T>(std::forward<Args>(args)...));
    }

    template <typename T>
    void Subscribe(void* owner, std::function<void(const T&)> handler) {
        SubscribeRaw(std::type_index(typeid(T)), owner, [handler](const Elysium::Message& msg) {
            handler(static_cast<const T&>(msg));
        });
    }
};

}  // namespace Elysium::Services
