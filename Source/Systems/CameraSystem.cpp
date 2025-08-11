#include "Systems/CameraSystem.h"

#include "Application.h"
#include "Scene.h"
#include "Entity.h"

#include "raylib.h"

namespace Elysium::Systems {

void CameraSystem::Update(float deltaTime) {
    // Update camera components that have follow behavior
    world->ForEachEntityWith<CameraComponent, FollowComponent, PositionComponent>([&](Entity entity) {
        auto& cameraComp = world->GetComponent<CameraComponent>(entity);
        auto& followComp = world->GetComponent<FollowComponent>(entity);
        auto& cameraPos = world->GetComponent<PositionComponent>(entity);

        // Find the target entity by name
        Entity targetEntity;
        if (world->GetEntityByName(followComp.targetEntityName, &targetEntity)) {
            if (world->HasComponent<PositionComponent>(targetEntity)) {
                auto& targetPos = world->GetComponent<PositionComponent>(targetEntity);

                // Lerp the camera position
                Vector2 currentPos = {cameraPos.x + cameraComp.position.x, cameraPos.y + cameraComp.position.y};
                Vector2 newPos = LerpVector2(currentPos, {targetPos.x, targetPos.y}, lerpSpeed_ * deltaTime);

                // Update camera position relative to entity position
                cameraComp.position = {newPos.x - cameraPos.x, newPos.y - cameraPos.y};
                
                // Update zoom
                cameraComp.zoom = 2.0f;
            }
        }
    });
}

void CameraSystem::BeginCameraMode() {
    // TODO: Implement camera mode for new camera system
}

void CameraSystem::EndCameraMode() {
    // TODO: Implement end camera mode for new camera system
}

Vector2 CameraSystem::LerpVector2(Vector2 start, Vector2 end, float t) {
    // Clamp t between 0 and 1
    t = (t < 0.0f) ? 0.0f : (t > 1.0f) ? 1.0f : t;

    return {
        start.x + (end.x - start.x) * t,
        start.y + (end.y - start.y) * t
    };
}

} // namespace Elysium::Systems
