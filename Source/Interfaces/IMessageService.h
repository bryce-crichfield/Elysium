#pragma once

#include <functional>
#include <memory>
#include <typeindex>
#include "Core/Message.h"
#include "Interfaces/IService.h"

namespace Elysium::Services {

class IMessageService : public IService {
   public:

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
