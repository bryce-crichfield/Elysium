#pragma once

#include <memory>
#include <string>
#include "Core/Entity.h"
#include "Service.h"
#include "Core/Timeline.h"

namespace Elysium {
class Scene;
}

namespace Elysium::Services {

class TimelineService : public Elysium::Service {
   public:
    TimelineService();
    ~TimelineService() = default;

    // Service interface
    void Initialize() override;
    void Shutdown() override;
    void Update(float deltaTime) override;
};

}  // namespace Elysium::Services
