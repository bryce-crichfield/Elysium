#include "Systems/CameraSystem.h"

#include "Application.h"
#include "Scene.h"
#include "Entity.h"

#include "raylib.h"

namespace Elysium::Systems {

void CameraSystem::Update(float deltaTime) {
    // Reset active camera
    activeCamera_ = nullptr;

    // Find all entities with CameraComponent
    world->ForEachEntityWith<CameraComponent, PositionComponent>([&](Entity cameraEntity) {
        auto& cameraComp = world->GetComponent<CameraComponent>(cameraEntity);
        auto& cameraPos = world->GetComponent<PositionComponent>(cameraEntity);

        // Find the target entity by name
        Entity targetEntity;
        if (world->GetEntityByName(cameraComp.target, &targetEntity)) {
            if (world->HasComponent<PositionComponent>(targetEntity)) {
                auto& targetPos = world->GetComponent<PositionComponent>(targetEntity);

                // Lerp the camera target directly (no separate position calculation)
                Vector2 currentTarget = cameraComp.camera.target;
                Vector2 newTarget = LerpVector2(currentTarget, {targetPos.x, targetPos.y}, lerpSpeed_ * deltaTime);

                int bufferWidth = application->GetConfig().framebufferWidth;
                int bufferHeight = application->GetConfig().framebufferHeight;

                // Update the raylib Camera2D
                cameraComp.camera.target = newTarget;
                cameraComp.camera.offset = {bufferWidth / 2.0f, bufferHeight / 2.0f}; // Use framebuffer dimensions, not screen
                cameraComp.camera.rotation = 0.0f;
                cameraComp.camera.zoom = 2.0f;

                // Set as active camera (use the first one found)
                if (activeCamera_ == nullptr) {
                    activeCamera_ = &cameraComp.camera;
                }
            }
        }
    });
}

void CameraSystem::BeginCameraMode() {
    if (activeCamera_ != nullptr) {
        BeginMode2D(*activeCamera_);
    }
}

void CameraSystem::EndCameraMode() {
    if (activeCamera_ != nullptr) {
        EndMode2D();
    }
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
