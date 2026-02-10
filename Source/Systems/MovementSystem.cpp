#include "Systems/MovementSystem.h"
#include "Core/SystemRegistry.h"
#include "Systems/SpatialSystem.h"
#include "Core/Component.h"
#include "Core/Entity.h"
#include "Core/Scene.h"
#include "raymath.h"
#include <algorithm>
#include "Components/MovementComponent.h"
#include "Components/PositionComponent.h"
#include "Components/BoundsComponent.h"
#include "Components/KinematicsComponent.h"
#include "Components/DirectionComponent.h"
#include "Components/FollowComponent.h"
namespace Elysium::Systems {

float MathLerp(float start, float end, float t) {
    return start + t * (end - start);
}

void MovementSystem::Update(float deltaTime) {
    if (!spatialSystem_) {
        spatialSystem_ = scene->GetSystem<SpatialSystem>();
    }

    world->Query<PositionComponent, FollowComponent>(
        [&](Entity e, auto& pos, auto& follow) {
            Entity targetEntity;
            if (!world->GetEntityByName(follow.targetEntityName, &targetEntity)) {
                return;
            }

            if (!world->HasComponent<PositionComponent>(targetEntity)) {
                return;
            }

            auto& targetPos = world->GetComponent<PositionComponent>(targetEntity);
            pos.x = MathLerp(pos.x, targetPos.x, follow.speed * deltaTime);
            pos.y = MathLerp(pos.y, targetPos.y, follow.speed * deltaTime);
        }
    );
}
}  // namespace Elysium::Systems

REGISTER_SYSTEM(Elysium::Systems::MovementSystem)
