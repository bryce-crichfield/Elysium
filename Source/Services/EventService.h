#pragma once

#include <vector>
#include "Core/Event.h"
#include "Service.h"

namespace Elysium::Services {

class EventService : public Elysium::Service {
   public:
    EventService();
    ~EventService() = default;

    // Service interface
    void Initialize() override;
    void Shutdown() override;
    void Update(float deltaTime) override;

    // Register/unregister listeners
    void RegisterListener(IEventListener* listener);
    void UnregisterListener(IEventListener* listener);

    // Dispatch event to all listeners
    void Dispatch(Event& event);

   private:
    std::vector<IEventListener*> listeners_;
};

}  // namespace Elysium::Services
