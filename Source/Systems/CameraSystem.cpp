#include "Systems/CameraSystem.h"
#include "Core/SystemRegistry.h"
#include "Core/Input.h"
#include "Interfaces/IApplicationService.h"
#include "Interfaces/ISceneService.h"
#include "Services/LogService.h"
#include "Services/SceneService.h"
#include "Core/Components/TransformComponent.h"
#include "Core/Components/CameraComponent.h"
#include "Core/Components/FollowComponent.h"

namespace Elysium::Systems {

void CameraSystem::OnEvent(Event& event) {
    IMouseListener::DispatchMouseEvent(event);
    IKeyboardListener::DispatchKeyboardEvent(event);
}

void CameraSystem::OnMouseButtonPressed(MouseButtonPressedEvent& event) {
    if (event.GetButton() == static_cast<int>(MouseButton::Middle)) {
        isDragging_ = true;
    }
}

void CameraSystem::OnMouseButtonReleased(MouseButtonReleasedEvent& event) {
    if (event.GetButton() == static_cast<int>(MouseButton::Middle)) {
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
    switch (static_cast<Key>(event.GetKey())) {
        case Key::W: keyW_ = true; break;
        case Key::A: keyA_ = true; break;
        case Key::S: keyS_ = true; break;
        case Key::D: keyD_ = true; break;
        default: break;
    }
}

void CameraSystem::OnKeyReleased(KeyReleasedEvent& event) {
    switch (static_cast<Key>(event.GetKey())) {
        case Key::W: keyW_ = false; break;
        case Key::A: keyA_ = false; break;
        case Key::S: keyS_ = false; break;
        case Key::D: keyD_ = false; break;
        default: break;
    }
}

void CameraSystem::Update(float deltaTime) {
    const float edgeThreshold = 15.0f;
    const float panSpeed = 600.0f;

    auto& sceneService = services->Get<Services::ISceneService>();
    const auto& config = services->Get<Services::IApplicationService>().GetConfig();

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

    if (panDir.Length() > 0.0f) {
        panDir = panDir.Normalized();
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
         world->Query<TransformComponent, CameraComponent>([&](Entity entity, auto& transform, auto& cam) {
             // Break follow when the user pans manually. Also detach from the
             // hierarchy: TransformSystem still composes local+parent for any
             // structural hierarchy child regardless of FollowComponent, so
             // leaving ParentComponent/childrenMap_ intact would keep re-gluing
             // the camera to its old follow target's position every frame.
             // Preserve the current composed world position as the new local
             // position before detaching, since local==world once it's a root.
             if (world->HasComponent<FollowComponent>(entity)) {
                 world->RemoveComponent<FollowComponent>(entity);
                 Entity parent = world->GetParent(entity);
                 if (parent != INVALID_ENTITY) {
                     transform.localX = transform.worldX;
                     transform.localY = transform.worldY;
                     world->RemoveChild(parent, entity);
                 }
             }

             if (panDir.x != 0 || panDir.y != 0) {
                 transform.localX += panDir.x * panSpeed * deltaTime;
                 transform.localY += panDir.y * panSpeed * deltaTime;
             }

             if (isDragging_) {
                 float zoomFactor = (cam.zoom > 0) ? (1.0f / cam.zoom) : 1.0f;
                 transform.localX -= dragDelta.x * zoomFactor;
                 transform.localY -= dragDelta.y * zoomFactor;
             }
         });
    }
    // Follow is handled by FollowSystem for entities with FollowComponent + ParentComponent
}


}  // namespace Elysium::Systems

REGISTER_SYSTEM(Elysium::Systems::CameraSystem)
