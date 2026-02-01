#include "Systems/CommandSystem.h"
#include "Core/Application.h"
#include "Core/Component.h"
#include "Core/Entity.h"
#include "Core/Scene.h"
#include "Services/LogService.h"
#include "raymath.h"
#include "Components/BoundsComponent.h"
#include "Components/FactionComponent.h"
#include "Components/SelectionComponent.h"
#include "Components/AttackComponent.h"
#include "Components/CameraComponent.h"
#include "Components/PositionComponent.h"
#include "Components/MovementComponent.h"
#include "Components/PathRequestComponent.h"

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

    Entity clickedEntity = FindEntityAtPoint(worldPos);
    bool isEnemy = false;

    if (clickedEntity != 0 && world->HasComponent<FactionComponent>(clickedEntity)) {
        auto& faction = world->GetComponent<FactionComponent>(clickedEntity);
        if (faction.name == "Enemy") {
            isEnemy = true;
        }
    }

    if (isEnemy) {
        IssueAttackCommand(clickedEntity);
    } else {
        IssueMoveCommand(worldPos);
    }
}

void CommandSystem::OnMouseButtonReleased(MouseButtonReleasedEvent& event) {
    // Could be used for command confirmation or cancellation
}

Entity CommandSystem::FindEntityAtPoint(Vector2 worldPos) {
    Entity foundEntity = 0;
    // Simple reverse iteration or z-index check would be better, but for now just find first
    world->Query<BoundsComponent>([&](Entity entity, auto& bounds) {
        if (CheckCollisionPointRec(worldPos, bounds.bounds)) {
            foundEntity = entity;
        }
    });
    return foundEntity;
}

void CommandSystem::IssueAttackCommand(Entity targetEntity) {
    int commandedCount = 0;

    world->Query<SelectionComponent, AttackComponent>([&](Entity entity, auto& sel, auto& attack) {
        attack.targetId = targetEntity;
        attack.isAttacking = true;
        
        LOG_INFOF("CommandSystem", "Entity %zu ordered to attack %zu", entity, targetEntity);
        commandedCount++;
    });
    
    if (commandedCount > 0) {
         LOG_INFOF("CommandSystem", "Issued attack command to %d units", commandedCount);
    }
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
            // Cancel any attack
            if (world->HasComponent<AttackComponent>(entity)) {
                auto& attack = world->GetComponent<AttackComponent>(entity);
                attack.isAttacking = false;
                attack.targetId = 0;
            }

            LOG_INFOF("CommandSystem", "Entity %zu selected at (%.1f, %.1f) issuing move to (%.1f, %.1f)",
                      entity, pos.x, pos.y, targetPos.x, targetPos.y);
            
            // Attach or update PathRequestComponent to trigger pathfinding
            if (world->HasComponent<PathRequestComponent>(entity)) {
                world->GetComponent<PathRequestComponent>(entity).target = targetPos;
            } else {
                world->AddComponent(entity, PathRequestComponent(targetPos));
            }
            
            commandedCount++;
        });

    if (commandedCount > 0) {
        LOG_INFOF("CommandSystem", "Issued move command to %d units -> (%.1f, %.1f)",
                  commandedCount, targetPos.x, targetPos.y);
    }
}

}  // namespace Elysium::Systems
