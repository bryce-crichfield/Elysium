#include "Services/EventService.h"
#include "Common.h"
#include "Scene.h"
#include "Services/LogService.h"

namespace Elysium::Services {

EventService::EventService() {
    name_ = "EventService";
    hasUi_ = false;  // No UI for now
}

void EventService::Initialize() {
    Profile;
    listeners_.clear();
    LOG_INFO("EventService", "Initialized");
}

void EventService::Shutdown() {
    Profile;
    listeners_.clear();
    LOG_INFO("EventService", "Shutdown");
}

void EventService::Update(float deltaTime) {
    Profile;
    // Event service doesn't need per-frame updates
    // Events are dispatched on-demand
}

void EventService::RegisterListener(Scene* scene) {
    if (!scene)
        return;

    // Check if already registered
    for (auto* listener : listeners_) {
        if (listener == scene) {
            return;  // Already registered
        }
    }

    listeners_.push_back(scene);
    LOG_DEBUGF("EventService", "Registered listener: %p", scene);
}

void EventService::UnregisterListener(Scene* scene) {
    if (!scene)
        return;

    for (auto it = listeners_.begin(); it != listeners_.end(); ++it) {
        if (*it == scene) {
            listeners_.erase(it);
            LOG_DEBUGF("EventService", "Unregistered listener: %p", scene);
            return;
        }
    }
}

void EventService::Dispatch(Event& event) {
    Profile;
    for (auto* listener : listeners_) {
        if (event.handled)
            break;  // Stop propagation if handled
        listener->OnEvent(event);
    }
}

}  // namespace Elysium::Services
