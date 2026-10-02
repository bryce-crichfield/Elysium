#pragma once
#include "Core/System.h"

namespace Elysium::Systems {

// Stands units on the ground: each root entity with a MovementComponent gets the height
// (Transform z) of the highest walkable model (ModelComponent::walkable) under its feet,
// found by casting a ray straight down from a step above where it stands now, so it climbs
// stairs and ramps but doesn't jump onto a roof overhead. Nothing below: the ground plane, 0.
// Runs after movement and before TransformSystem.
class GroundSystem : public System {
   public:
    using System::System;

    void Update(float deltaTime) override;
    bool RunsWhenPaused() const override { return true; }  // a unit dragged in the editor lands too

   protected:
    SystemParameters DefaultParameters() const override;
};

}  // namespace Elysium::Systems
