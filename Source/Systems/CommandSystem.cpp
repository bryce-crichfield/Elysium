#include "Systems/CommandSystem.h"
#include "Core/Application.h"
#include "Core/Component.h"
#include "Core/Entity.h"
#include "Core/Scene.h"
#include "Services/LogService.h"
#include "raymath.h"

namespace Elysium::Systems {

CommandSystem::CommandSystem(Context context) : System(context) {
}

void CommandSystem::Update(float deltaTime) {
    // Command logic is event-driven
}

void CommandSystem::OnEvent(Event& event) {
    DispatchMouseEvent(event);
}

void CommandSystem::OnMouseButtonPressed(MouseButtonPressedEvent& event) {
    // Right-click issues move command
    if (event.GetButton() != MOUSE_RIGHT_BUTTON) {
        return;
    }

    Vector2 fbPos = event.GetPosition();
    Vector2 worldPos = FramebufferToWorld(fbPos);

    IssueMoveCommand(worldPos);
}

void CommandSystem::OnMouseButtonReleased(MouseButtonReleasedEvent& event) {
    // Could be used for command confirmation or cancellation
}

Vector2 CommandSystem::FramebufferToWorld(Vector2 fbPos) {
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
        return fbPos;
    }

    Vector2 viewportCenter = {
        camera->viewport.width * 0.5f,
        camera->viewport.height * 0.5f};

    Vector2 centered = {
        fbPos.x - viewportCenter.x,
        fbPos.y - viewportCenter.y};

    Vector2 unscaled = {
        centered.x / camera->zoom,
        centered.y / camera->zoom};

    Vector2 worldPos = {
        unscaled.x + cameraPos.x,
        unscaled.y + cameraPos.y};

    return worldPos;
}

void CommandSystem::IssueMoveCommand(Vector2 targetPos) {
    int commandedCount = 0;

    // Query entities that have SelectionComponent (are selected) and can move
    world->Query<SelectionComponent, MovementComponent, PositionComponent>(
        [&](Entity entity, auto& sel, auto& movement, auto& pos) {
            LOG_INFOF("CommandSystem", "Entity %zu selected at (%.1f, %.1f) issuing move to (%.1f, %.1f)",
                      entity, pos.x, pos.y, targetPos.x, targetPos.y);
            // Clear existing waypoints and set new destination
            movement.ClearWaypoints();
            movement.AddWaypoint(targetPos);
            movement.currentWaypointIndex = 0;
            movement.isMoving = true;
            movement.loop = false;  // Don't loop for move commands

            commandedCount++;
        });

    if (commandedCount > 0) {
        LOG_INFOF("CommandSystem", "Issued move command to %d units -> (%.1f, %.1f)",
                  commandedCount, targetPos.x, targetPos.y);
    }
}

}  // namespace Elysium::Systems
