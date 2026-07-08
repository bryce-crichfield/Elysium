#pragma once
#include "Core/System.h"
#include "Core/Entity.h"
#include <cstdint>

namespace Elysium {
    struct TransformComponent;
}

namespace Elysium::Systems {

// Walks the entity hierarchy (ParentComponent / World::GetChildren) once per
// frame and composes each entity's local TransformComponent with its parent's
// already-composed world transform, caching the result on worldX/worldY/... .
// Roots (no parent, or parent has no TransformComponent) simply copy local -> world.
class TransformSystem : public System {
   public:
    using System::System;

    void Update(float deltaTime) override;
    bool RunsWhenPaused() const override { return true; }

   private:
    void ComposeRecursive(Entity entity, const TransformComponent* parentWorld, uint32_t depth);
};

}  // namespace Elysium::Systems
