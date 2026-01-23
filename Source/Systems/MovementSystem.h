#pragma once
#include "Core/System.h"

namespace Elysium::Systems {

class SpatialSystem;

class MovementSystem : public System {
   private:
    SpatialSystem* spatialSystem_ = nullptr;

   public:
    MovementSystem(Context context) : System(context) {}

    void Update(float deltaTime) override;
};
}  // namespace Elysium::Systems
