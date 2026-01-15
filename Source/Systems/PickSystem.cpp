#include "Systems/PickSystem.h"
#include "Component.h"
#include "Entity.h"
#include "Services/LogService.h"
#include "raymath.h"

namespace Elysium::Systems {

PickSystem::PickSystem(Context context) : System(context) {}

void PickSystem::Update(float deltaTime) {
    // Nothing to update per frame - all logic is event-driven
}

void PickSystem::OnEvent(Event& event) {
    if (event.Is<MouseButtonPressedEvent>()) {
        HandleMousePressed(*event.As<MouseButtonPressedEvent>());
    }
    else if (event.Is<MouseMovedEvent>()) {
        HandleMouseMoved(*event.As<MouseMovedEvent>());
    }
    else if (event.Is<MouseButtonReleasedEvent>()) {
        HandleMouseReleased(*event.As<MouseButtonReleasedEvent>());
    }
}

void PickSystem::HandleMousePressed(const MouseButtonPressedEvent& event) {
    // Only handle left mouse button
    if (event.GetButton() != MOUSE_LEFT_BUTTON) {
        return;
    }

    Vector2 fbPos = event.GetPosition();
    Vector2 worldPos = FramebufferToWorld(fbPos);

    // Find topmost entity under mouse (reverse iteration for top-to-bottom)
    Entity selectedEntity = 0;
    bool foundEntity = false;

    world->Query<BoundsComponent, PositionComponent>([&](Entity entity, auto& bounds, auto& pos) {
        if (IsPointInBounds(worldPos, bounds.bounds)) {
            selectedEntity = entity;
            foundEntity = true;

            // Calculate offset from entity position to mouse position
            dragOffset_.x = worldPos.x - pos.x;
            dragOffset_.y = worldPos.y - pos.y;
        }
    });

    if (foundEntity) {
        draggedEntity_ = selectedEntity;
        isDragging_ = true;

        // Mark entity as dragging
        auto& bounds = world->GetComponent<BoundsComponent>(draggedEntity_);
        bounds.isDragging = true;

        LOG_INFOF("PickSystem", "Started dragging entity %zu at world pos (%.1f, %.1f)",
                  draggedEntity_, worldPos.x, worldPos.y);
    }
}

void PickSystem::HandleMouseMoved(const MouseMovedEvent& event) {
    if (!isDragging_ || draggedEntity_ == 0) {
        return;
    }

    // Update entity position to follow mouse
    Vector2 fbPos = event.GetPosition();
    Vector2 worldPos = FramebufferToWorld(fbPos);
    auto& pos = world->GetComponent<PositionComponent>(draggedEntity_);

    pos.x = worldPos.x - dragOffset_.x;
    pos.y = worldPos.y - dragOffset_.y;
}

void PickSystem::HandleMouseReleased(const MouseButtonReleasedEvent& event) {
    // Only handle left mouse button
    if (event.GetButton() != MOUSE_LEFT_BUTTON) {
        return;
    }

    if (isDragging_ && draggedEntity_ != 0) {
        // Clear dragging flag
        auto& bounds = world->GetComponent<BoundsComponent>(draggedEntity_);
        bounds.isDragging = false;

        LOG_INFOF("PickSystem", "Stopped dragging entity %zu", draggedEntity_);

        draggedEntity_ = 0;
        isDragging_ = false;
    }
}

bool PickSystem::IsPointInBounds(Vector2 point, const Rectangle& bounds) {
    return CheckCollisionPointRec(point, bounds);
}

Vector2 PickSystem::FramebufferToWorld(Vector2 fbPos) {
    // Find the active camera
    Entity cameraEntity = 0;
    CameraComponent* camera = nullptr;
    Vector2 cameraPos = {0, 0};

    world->Query<CameraComponent, PositionComponent>([&](Entity entity, auto& cam, auto& pos) {
        if (cam.isVisible) {
            cameraEntity = entity;
            camera = &cam;
            cameraPos = {pos.x, pos.y};
        }
    });

    if (!camera) {
        // No camera found, return framebuffer coords as-is
        return fbPos;
    }

    // Transform from framebuffer to world space (inverse of render transform)
    // Render transform: world -> camera translation -> scale -> center in viewport
    // Inverse: uncenter from viewport -> unscale -> uncamera translation

    Vector2 viewportCenter = {
        camera->viewport.width * 0.5f,
        camera->viewport.height * 0.5f
    };

    // 1. Subtract viewport center (uncenter)
    Vector2 centered = {
        fbPos.x - viewportCenter.x,
        fbPos.y - viewportCenter.y
    };

    // 2. Divide by zoom (unscale)
    Vector2 unscaled = {
        centered.x / camera->zoom,
        centered.y / camera->zoom
    };

    // 3. Add camera position (uncamera translation)
    Vector2 worldPos = {
        unscaled.x + cameraPos.x,
        unscaled.y + cameraPos.y
    };

    return worldPos;
}

} // namespace Elysium::Systems
