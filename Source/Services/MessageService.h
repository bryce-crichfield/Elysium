#pragma once

#include <functional>
#include <typeindex>
#include <unordered_map>
#include "Core/Message.h"
#include "Core/ServiceLocator.h"
#include "Interfaces/IMessageService.h"

namespace Elysium::Services {

class MessageService : public IMessageService {
   public:
    MessageService(ServiceLocator& registry);
    ~MessageService() = default;

    // Service interface
    void Initialize() override;
    void Shutdown() override;
    void Update(float deltaTime) override;

    void PostRaw(std::unique_ptr<Elysium::Message> message) override;
    void SubscribeRaw(std::type_index type, void* owner,
                       std::function<void(const Elysium::Message&)> handler) override;

    // Remove all subscriptions for an owner (call in OnExit/Shutdown)
    void UnsubscribeAll(void* owner) override;

   private:
    // Type-erased handler that can be called with a Message*
    struct HandlerEntry {
        void* owner;
        std::function<void(const Message&)> handler;
    };

    ServiceLocator& registry_;
    MessageQueue queue_;
    std::unordered_map<std::type_index, std::vector<HandlerEntry>> handlers_;
    std::mutex handlersMutex_;  // Protects handlers_ map

    // Stats for ImGui
    size_t messagesProcessedThisFrame_ = 0;
    size_t totalMessagesProcessed_ = 0;
};

}  // namespace Elysium::Services
