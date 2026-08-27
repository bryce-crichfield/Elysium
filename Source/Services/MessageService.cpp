#include "Services/MessageService.h"
#include <typeindex>
#include "Interfaces/ISceneService.h"
#include "Services/SceneService.h"

namespace Elysium::Services {

MessageService::MessageService(ServiceLocator& registry) : Service(registry) {
}

// Service interface
void MessageService::Initialize() {
}

void MessageService::Shutdown() {
}

void MessageService::PostRaw(std::unique_ptr<Elysium::Message> message) {
    queue_.Push(std::move(message));
}

void MessageService::SubscribeRaw(std::type_index type, void* owner,
                                   std::function<void(const Elysium::Message&)> handler) {
    std::lock_guard<std::mutex> lock(handlersMutex_);
    handlers_[type].push_back({owner, std::move(handler)});
}

void MessageService::Update(float deltaTime) {
    auto messages = queue_.Swap();

    if (messages.empty())
        return;

    messagesProcessedThisFrame_ = messages.size();
    totalMessagesProcessed_ += messages.size();

    for (auto& msg : messages) {
        if (registry_.Has<ISceneService>()) {
            registry_.Get<ISceneService>().OnMessage(*msg);
        }

        // Dispatch to subscribed handlers
        {
            std::lock_guard<std::mutex> lock(handlersMutex_);
            auto it = handlers_.find(std::type_index(typeid(*msg)));
            if (it != handlers_.end()) {
                for (const auto& entry : it->second) {
                    entry.handler(*msg);
                }
            }
        }
    }
}


// Remove all subscriptions for an owner (call in OnExit/Shutdown)
void MessageService::UnsubscribeAll(void* owner) {
    std::lock_guard<std::mutex> lock(handlersMutex_);

    // Iterate through all message types in the map
    for (auto& [type, entries] : handlers_) {
        // Remove all handlers belonging to this specific owner
        entries.erase(
            std::remove_if(entries.begin(), entries.end(),
                           [owner](const HandlerEntry& entry) {
                               return entry.owner == owner;
                           }),
            entries.end());
    }
}
}  // namespace Elysium::Services