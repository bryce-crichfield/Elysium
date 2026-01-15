#pragma once

#include "Service.h"
#include "Event.h"
#include <vector>
#include <functional>

namespace Elysium {
    class Scene;
}

namespace Elysium::Services {

// EventListener is just a callback
using EventListener = std::function<void(Event&)>;

class EventService : public Elysium::Service {
public:
    EventService();
    ~EventService() = default;

    // Service interface
    void Initialize() override;
    void Shutdown() override;
    void Update(float deltaTime) override;

    // Register/unregister listeners (using raw pointers to avoid lifetime hell)
    void RegisterListener(Scene* scene);
    void UnregisterListener(Scene* scene);

    // Dispatch event to all listeners
    void Dispatch(Event& event);

private:
    std::vector<Scene*> listeners_;
};

} // namespace Elysium::Services
