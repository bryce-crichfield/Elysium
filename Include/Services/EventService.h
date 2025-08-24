#pragma once

#include "Service.h"
#include <vector>
#include <functional>
#include <typeindex>
#include <unordered_map>
#include <memory>

namespace Elysium::Services {

class EventService : public Elysium::Service {
public:
    EventService();
    ~EventService() = default;

    // Service interface
    void Initialize() override;
    void Shutdown() override;
    void Update(float deltaTime) override;
    void OnDebugDraw() override;

    // Event system functionality
    template<typename T>
    using EventHandler = std::function<void(const T&)>;
    
    template<typename T>
    void Subscribe(EventHandler<T> handler) {
        auto typeIndex = std::type_index(typeid(T));
        auto wrapper = [handler](const void* event) {
            handler(*static_cast<const T*>(event));
        };
        listeners[typeIndex].push_back(wrapper);
    }
    
    template<typename T>
    void FireEvent(const T& event) {
        auto typeIndex = std::type_index(typeid(T));
        auto it = listeners.find(typeIndex);
        if (it != listeners.end()) {
            for (const auto& handler : it->second) {
                handler(&event);
            }
            eventCounts_[typeIndex]++;
            totalEventsProcessed_++;
        }
    }

private:
    using HandlerWrapper = std::function<void(const void*)>;
    std::unordered_map<std::type_index, std::vector<HandlerWrapper>> listeners;
    
    // Debug/stats tracking
    size_t totalEventsProcessed_ = 0;
    std::unordered_map<std::type_index, size_t> eventCounts_;
};

} // namespace Elysium::Services
