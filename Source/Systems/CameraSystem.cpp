#include "Systems/CameraSystem.h"

#include "Core/Application.h"
#include "Core/Entity.h"
#include "Core/Scene.h"
#include "Services/SceneService.h"

#include "raylib.h"
#include "raymath.h"

namespace Elysium::Systems {

void CameraSystem::Update(float deltaTime) {
    // RTS Camera Control
    const float edgeThreshold = 15.0f;
    const float panSpeed = 600.0f;
    
    auto& sceneService = Application::GetInstance().GetService<Services::SceneService>();
    Rectangle viewBounds = sceneService.GetLetterboxRect();
    Vector2 mousePos = GetMousePosition();

    Vector2 panDir = {0, 0};
    bool shouldPan = false;

    // Only pan if mouse is inside the view area OR if we are focused and outside (infinite edge)
    // We check IsWindowFocused to prevent panning while alt-tabbed
    if (IsWindowFocused()) {
        // Edge Panning
        // Left
        if (mousePos.x <= viewBounds.x + edgeThreshold) {
            panDir.x -= 1.0f;
            shouldPan = true;
        } 
        // Right
        else if (mousePos.x >= viewBounds.x + viewBounds.width - edgeThreshold) {
            panDir.x += 1.0f;
            shouldPan = true;
        }

        // Up
        if (mousePos.y <= viewBounds.y + edgeThreshold) {
            panDir.y -= 1.0f;
            shouldPan = true;
        } 
        // Down
        else if (mousePos.y >= viewBounds.y + viewBounds.height - edgeThreshold) {
            panDir.y += 1.0f;
            shouldPan = true;
        }

        // WASD Panning
        if (IsKeyDown(KEY_W)) { panDir.y -= 1.0f; shouldPan = true; }
        if (IsKeyDown(KEY_S)) { panDir.y += 1.0f; shouldPan = true; }
        if (IsKeyDown(KEY_A)) { panDir.x -= 1.0f; shouldPan = true; }
        if (IsKeyDown(KEY_D)) { panDir.x += 1.0f; shouldPan = true; }
    }
    
    // Normalize vector to ensure consistent speed
    if (Vector2Length(panDir) > 0.0f) {
        panDir = Vector2Normalize(panDir);
    }

    // Middle Mouse Drag Panning
    bool isDragging = IsMouseButtonDown(MOUSE_BUTTON_MIDDLE);
    Vector2 dragDelta = {0, 0};
    if (isDragging) {
        dragDelta = GetMouseDelta();
        shouldPan = true;
    }

    // Apply Panning
    if (shouldPan) {
         world->Query<PositionComponent, CameraComponent>([&](Entity entity, auto& pos, auto& cam) {
             // Apply Directional Pan (Edge + WASD)
             if (panDir.x != 0 || panDir.y != 0) {
                 pos.x += panDir.x * panSpeed * deltaTime;
                 pos.y += panDir.y * panSpeed * deltaTime;
             }
             
             // Apply Drag Pan
             if (isDragging) {
                 float zoomFactor = (cam.zoom > 0) ? (1.0f / cam.zoom) : 1.0f;
                 pos.x -= dragDelta.x * zoomFactor;
                 pos.y -= dragDelta.y * zoomFactor;
             }
             
             // If we are manually panning, remove the FollowComponent so it doesn't snap back
             if (world->HasComponent<FollowComponent>(entity)) {
                 world->RemoveComponent<FollowComponent>(entity);
             }
         });
    } else {
        // Only do follow logic if we aren't panning
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
    }

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
