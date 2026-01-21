#include "Systems/CameraSystem.h"

#include "Core/Application.h"
#include "Core/Entity.h"
#include "Core/Scene.h"

#include "raylib.h"

namespace Elysium::Systems {

void CameraSystem::Update(float deltaTime) {
    // Update camera components that have follow behavior
    world->Query<PositionComponent, CameraComponent, FollowComponent>([&](Entity entity, auto& positionComp, auto& cameraComp, auto& followComp) {
        // Find the target entity by name
        Entity targetEntity;
        if (world->GetEntityByName(followComp.targetEntityName, &targetEntity)) {
            if (world->HasComponent<PositionComponent>(targetEntity)) {
                auto& targetPos = world->GetComponent<PositionComponent>(targetEntity);

                // Lerp the camera position to follow the target
                Vector2 targetCameraPos = {targetPos.x, targetPos.y};
                Vector2 currentCameraPos = {positionComp.x, positionComp.y};
                Vector2 newCameraPos = LerpVector2(currentCameraPos, targetCameraPos, followComp.speed * deltaTime);

                // Update camera position directly
                positionComp.x = newCameraPos.x;
                positionComp.y = newCameraPos.y;
            }
        }
    });

    // Note: Viewport setup should be handled by the scene/application
    // We don't modify viewport here since we respect framebuffer rendering
}

Vector2 CameraSystem::LerpVector2(Vector2 start, Vector2 end, float t) {
    // Clamp t between 0 and 1
    t = (t < 0.0f) ? 0.0f : (t > 1.0f) ? 1.0f
                                       : t;

    return {
        start.x + (end.x - start.x) * t,
        start.y + (end.y - start.y) * t};
}

}  // namespace Elysium::Systems
