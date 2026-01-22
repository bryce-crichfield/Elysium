#include "Services/EventService.h"
#include "Core/Common.h"
#include "Services/LogService.h"

namespace Elysium::Services {

EventService::EventService() {
    name_ = "EventService";
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

void EventService::RegisterListener(IEventListener* listener) {
    if (!listener)
        return;

    // Check if already registered
    for (auto* l : listeners_) {
        if (l == listener) {
            return;  // Already registered
        }
    }

    listeners_.push_back(listener);
    LOG_DEBUGF("EventService", "Registered listener: %p", listener);
}

void EventService::UnregisterListener(IEventListener* listener) {
    if (!listener)
        return;

    for (auto it = listeners_.begin(); it != listeners_.end(); ++it) {
        if (*it == listener) {
            listeners_.erase(it);
            LOG_DEBUGF("EventService", "Unregistered listener: %p", listener);
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
