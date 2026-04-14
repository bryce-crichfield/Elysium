#include "Systems/FollowSystem.h"
#include "Core/SystemRegistry.h"
#include "Components/PositionComponent.h"
#include "Components/FollowComponent.h"
#include "Components/ParentComponent.h"

namespace Elysium::Systems {

void FollowSystem::Update(float deltaTime) {
    world->Query<PositionComponent, FollowComponent, ParentComponent>([&](Entity entity, auto& pos, auto& follow, auto& parentComp) {
        Entity parent = parentComp.parent;
        if (parent == INVALID_ENTITY) return;
        if (!world->HasComponent<PositionComponent>(parent)) return;

        auto& targetPos = world->GetComponent<PositionComponent>(parent);
        float targetX = targetPos.x + follow.offsetX;
        float targetY = targetPos.y + follow.offsetY;

        if (follow.speed <= 0.0f) {
            pos.x = targetX;
            pos.y = targetY;
        } else {
            pos.x += (targetX - pos.x) * follow.speed * deltaTime;
            pos.y += (targetY - pos.y) * follow.speed * deltaTime;
        }
    });
}

}  // namespace Elysium::Systems

REGISTER_SYSTEM(Elysium::Systems::FollowSystem)
