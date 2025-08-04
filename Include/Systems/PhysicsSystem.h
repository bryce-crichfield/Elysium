#pragma once

#include "Entity.h"

namespace Elysium::Systems {

class PhysicsSystem : public System {
public:
    PhysicsSystem(EntityWorld* world, float gravity = 500.0f) : System(world), gravity_(gravity) {}
    
    void Update(float deltaTime) override;
    void SetGravity(float gravity) { gravity_ = gravity; }
    float GetGravity() const { return gravity_; }
    
private:
    float gravity_;
};

} // namespace Elysium::Systems