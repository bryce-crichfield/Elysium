#include "Systems/CommandSystem.h"
#include "Core/SystemRegistry.h"
#include "Core/Application.h"
#include "Core/Entity.h"
#include "Core/Scene.h"
#include "Services/LogService.h"
#include "Components/AttackComponent.h"
#include "Components/CameraComponent.h"
#include "Components/TransformComponent.h"
#include "Components/MovementComponent.h"
#include "Components/SelectionComponent.h"

namespace Elysium::Systems {

CommandSystem::CommandSystem(Context context) : System(context) {
}

void CommandSystem::Update(float deltaTime) {
    // Command logic is event-driven
}

void CommandSystem::OnEvent(Event& event) {
    // Skip input processing in Editor mode
    if (Application::GetInstance().GetMode() == AppMode::Editor) {
        return;
    }
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

    world->Query<CameraComponent, TransformComponent>([&](Entity entity, auto& cam, auto& transform) {
        if (cam.isVisible) {
            cameraEntity = entity;
            camera = &cam;
            cameraPos = {transform.worldX, transform.worldY};
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

    // TODO: Support issuing commands through CommandSystem.

    if (commandedCount > 0) {
        LOG_INFOF("CommandSystem", "Issued move command to %d units -> (%.1f, %.1f)",
                  commandedCount, targetPos.x, targetPos.y);
    }
}

}  // namespace Elysium::Systems

REGISTER_SYSTEM(Elysium::Systems::CommandSystem)
