#include "Systems/CameraSystem.h"
#include "Core/SystemRegistry.h"
#include "Core/Application.h"
#include "Services/LogService.h"
#include "Services/SceneService.h"
#include "Components/PositionComponent.h"
#include "Components/CameraComponent.h"
#include "Components/FollowComponent.h"
#include "raymath.h"

namespace Elysium::Systems {

void CameraSystem::OnEvent(Event& event) {
    // Skip input processing in Editor mode - let DebugSystem handle it
    if (Application::GetInstance().GetMode() == AppMode::Editor) {
        return;
    }
    IMouseListener::DispatchMouseEvent(event);
    IKeyboardListener::DispatchKeyboardEvent(event);
}

void CameraSystem::OnMouseButtonPressed(MouseButtonPressedEvent& event) {
    if (event.GetButton() == MOUSE_BUTTON_MIDDLE) {
        isDragging_ = true;
    }
}

void CameraSystem::OnMouseButtonReleased(MouseButtonReleasedEvent& event) {
    if (event.GetButton() == MOUSE_BUTTON_MIDDLE) {
        isDragging_ = false;
    }
}

void CameraSystem::OnMouseMoved(MouseMovedEvent& event) {
    mousePosition_ = event.GetPosition();
    mouseDelta_ = event.GetDelta();
}

void CameraSystem::OnMouseWheel(MouseWheelEvent& event) {}

void CameraSystem::OnMouseEnter(MouseEnterEvent& event) {
    hasFocus_ = true;
}

void CameraSystem::OnMouseExit(MouseExitEvent& event) {
    hasFocus_ = false;
}

void CameraSystem::OnKeyPressed(KeyPressedEvent& event) {
    switch (event.GetKey()) {
        case KEY_W: keyW_ = true; break;
        case KEY_A: keyA_ = true; break;
        case KEY_S: keyS_ = true; break;
        case KEY_D: keyD_ = true; break;
    }
}

void CameraSystem::OnKeyReleased(KeyReleasedEvent& event) {
    switch (event.GetKey()) {
        case KEY_W: keyW_ = false; break;
        case KEY_A: keyA_ = false; break;
        case KEY_S: keyS_ = false; break;
        case KEY_D: keyD_ = false; break;
    }
}

void CameraSystem::Update(float deltaTime) {
    const float edgeThreshold = 15.0f;
    const float panSpeed = 600.0f;

    auto& sceneService = Application::GetInstance().GetService<Services::SceneService>();
    const auto& config = Application::GetInstance().GetConfig();

    // mousePosition_ is in framebuffer space (0..fbWidth, 0..fbHeight)
    Rectangle viewBounds = {0, 0, (float)config.framebufferWidth, (float)config.framebufferHeight};

    Vector2 panDir = {0, 0};
    bool shouldPan = false;

    if (hasFocus_) {
        // Edge Panning
        if (mousePosition_.x <= edgeThreshold) {
            panDir.x -= 1.0f;
            shouldPan = true;
        } else if (mousePosition_.x >= viewBounds.width - edgeThreshold) {
            panDir.x += 1.0f;
            shouldPan = true;
        }

        if (mousePosition_.y <= edgeThreshold) {
            panDir.y -= 1.0f;
            shouldPan = true;
        } else if (mousePosition_.y >= viewBounds.height - edgeThreshold) {
            panDir.y += 1.0f;
            shouldPan = true;
        }

        // WASD Panning
        if (keyW_) { panDir.y -= 1.0f; shouldPan = true; }
        if (keyS_) { panDir.y += 1.0f; shouldPan = true; }
        if (keyA_) { panDir.x -= 1.0f; shouldPan = true; }
        if (keyD_) { panDir.x += 1.0f; shouldPan = true; }
    }

    if (Vector2Length(panDir) > 0.0f) {
        panDir = Vector2Normalize(panDir);
    }

    // Middle Mouse Drag Panning
    Vector2 dragDelta = {0, 0};
    if (isDragging_) {
        dragDelta = mouseDelta_;
        shouldPan = true;
    }

    // Reset mouse delta after consuming it
    mouseDelta_ = {0, 0};

    if (shouldPan) {
         world->Query<PositionComponent, CameraComponent>([&](Entity entity, auto& pos, auto& cam) {
             if (panDir.x != 0 || panDir.y != 0) {
                 pos.x += panDir.x * panSpeed * deltaTime;
                 pos.y += panDir.y * panSpeed * deltaTime;
             }

             if (isDragging_) {
                 float zoomFactor = (cam.zoom > 0) ? (1.0f / cam.zoom) : 1.0f;
                 pos.x -= dragDelta.x * zoomFactor;
                 pos.y -= dragDelta.y * zoomFactor;
             }

             if (world->HasComponent<FollowComponent>(entity)) {
                 world->RemoveComponent<FollowComponent>(entity);
             }
         });
    } else {
        world->Query<PositionComponent, CameraComponent, FollowComponent>([&](Entity entity, auto& positionComp, auto& cameraComp, auto& followComp) {
            Entity targetEntity;
            if (world->GetEntityByName(followComp.targetEntityName, &targetEntity)) {
                if (world->HasComponent<PositionComponent>(targetEntity)) {
                    auto& targetPos = world->GetComponent<PositionComponent>(targetEntity);

                    Vector2 targetCameraPos = {targetPos.x, targetPos.y};
                    Vector2 currentCameraPos = {positionComp.x, positionComp.y};
                    Vector2 newCameraPos = LerpVector2(currentCameraPos, targetCameraPos, followComp.speed * deltaTime);

                    positionComp.x = newCameraPos.x;
                    positionComp.y = newCameraPos.y;
                }
            }
        });
    }
}

Vector2 CameraSystem::LerpVector2(Vector2 start, Vector2 end, float t) {
    t = (t < 0.0f) ? 0.0f : (t > 1.0f) ? 1.0f : t;
    return {
        start.x + (end.x - start.x) * t,
        start.y + (end.y - start.y) * t};
}

}  // namespace Elysium::Systems

REGISTER_SYSTEM(Elysium::Systems::CameraSystem)
