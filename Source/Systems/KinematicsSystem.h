#pragma once
#include "Core/System.h"

namespace Elysium::Systems {

/**
 * KinematicsSystem integrates acceleration and velocity to update position.
 * Applies friction/damping to velocity.
 */
class KinematicsSystem : public System {
   public:
    KinematicsSystem(Context context) : System(context) {}

    void Update(float deltaTime) override;
};

}  // namespace Elysium::Systems
