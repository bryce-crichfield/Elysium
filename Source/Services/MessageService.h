#pragma once

#include <functional>
#include <typeindex>
#include <unordered_map>
#include "Core/Message.h"
#include "Service.h"

namespace Elysium::Services {

class MessageService : public Service {
   public:
    MessageService();
    ~MessageService() = default;

    // Service interface
    void Initialize() override;
    void Shutdown() override;
    void Update(float deltaTime) override;
    void ImGui() override;

    // Thread-safe: post a message from any thread
    template <typename T, typename... Args>
    void Post(Args&&... args);

    // Subscribe to a message type with owner for lifecycle management
    template <typename T>
    void Subscribe(void* owner, std::function<void(const T&)> handler);

    // Remove all subscriptions for an owner (call in OnExit/Shutdown)
    void UnsubscribeAll(void* owner);

   private:
    // Type-erased handler that can be called with a Message*
    struct HandlerEntry {
        void* owner;
        std::function<void(const Message&)> handler;
    };

    MessageQueue queue_;
    std::unordered_map<std::type_index, std::vector<HandlerEntry>> handlers_;
    std::mutex handlersMutex_;  // Protects handlers_ map

    // Stats for ImGui
    size_t messagesProcessedThisFrame_ = 0;
    size_t totalMessagesProcessed_ = 0;
};

// Template implementations
template <typename T, typename... Args>
void MessageService::Post(Args&&... args) {
    queue_.Push(std::make_unique<T>(std::forward<Args>(args)...));
}

template <typename T>
void MessageService::Subscribe(void* owner, std::function<void(const T&)> handler) {
    std::lock_guard<std::mutex> lock(handlersMutex_);

    // Wrap the typed handler in a type-erased wrapper
    auto wrapper = [handler](const Message& msg) {
        handler(static_cast<const T&>(msg));
    };

    handlers_[std::type_index(typeid(T))].push_back({owner, wrapper});
}

}  // namespace Elysium::Services
