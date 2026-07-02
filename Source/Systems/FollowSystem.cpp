#include "Systems/FollowSystem.h"
#include "Core/SystemRegistry.h"
#include "Components/TransformComponent.h"
#include "Components/FollowComponent.h"
#include "Components/ParentComponent.h"

namespace Elysium::Systems {

// Drives the entity's own local offset toward the follow target's offset;
// TransformSystem composes that local offset with the parent's world
// transform, so this system must never touch worldX/worldY itself (doing so
// would double-apply the offset once here and once in composition).
void FollowSystem::Update(float deltaTime) {
    world->Query<TransformComponent, FollowComponent, ParentComponent>([&](Entity entity, auto& t, auto& follow, auto& parentComp) {
        if (parentComp.parent == INVALID_ENTITY) return;

        if (follow.speed <= 0.0f) {
            t.localX = follow.offsetX;
            t.localY = follow.offsetY;
        } else {
            t.localX += (follow.offsetX - t.localX) * follow.speed * deltaTime;
            t.localY += (follow.offsetY - t.localY) * follow.speed * deltaTime;
        }
    });
}

}  // namespace Elysium::Systems

REGISTER_SYSTEM(Elysium::Systems::FollowSystem)
