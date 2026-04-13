#pragma once
#include "Core/System.h"

namespace Elysium::Systems {

// Resolves non-trigger AABB collisions detected by CollisionSystem.
// Must run after CollisionSystem and before ScriptSystem each frame.
// Trigger colliders (isTrigger=true) are skipped — scripts handle those.
class PhysicsResponseSystem : public System {
public:
    PhysicsResponseSystem(Context context) : System(context) {}
    void Update(float deltaTime) override;
};

}  // namespace Elysium::Systems
