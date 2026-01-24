#include "Systems/AttackSystem.h"
#include "Core/Component.h"
#include "Core/Entity.h"
#include "Core/Scene.h"
#include "Services/LogService.h"
#include "raymath.h"

namespace Elysium::Systems {

AttackSystem::AttackSystem(Context context) : System(context) {
}

void AttackSystem::Update(float deltaTime) {
    world->Query<AttackComponent, PositionComponent>([&](Entity entity, auto& attack, auto& pos) {
        // Decrease cooldown timer
        if (attack.timer > 0.0f) {
            attack.timer -= deltaTime;
        }

        if (!attack.isAttacking || attack.targetId == 0) {
            return;
        }

        // Validate target
        if (!world->HasComponent<PositionComponent>(attack.targetId) || 
            !world->HasComponent<HealthComponent>(attack.targetId)) {
            // Target dead or invalid
            attack.isAttacking = false;
            attack.targetId = 0;
            return;
        }

        auto& targetPos = world->GetComponent<PositionComponent>(attack.targetId);
        Vector2 myPosition = {pos.x, pos.y};
        Vector2 targetPosition = {targetPos.x, targetPos.y};

        float dist = Vector2Distance(myPosition, targetPosition);

        if (dist <= attack.range) {
            // IN RANGE
            
            // Stop moving if we were moving to engage
            // We check if we have movement component and if it's active
            if (world->HasComponent<MovementComponent>(entity)) {
                auto& move = world->GetComponent<MovementComponent>(entity);
                if (move.isMoving) {
                    move.ClearWaypoints();
                    move.isMoving = false;
                    
                    // Also clear kinematics if present to stop sliding
                    if (world->HasComponent<KinematicsComponent>(entity)) {
                        auto& kin = world->GetComponent<KinematicsComponent>(entity);
                        kin.velocity = {0, 0};
                        kin.acceleration = {0, 0};
                    }
                }
            }
            
            // Fire if cooldown ready
            if (attack.timer <= 0.0f) {
                SpawnProjectile(entity, myPosition, attack.targetId, attack.damage);
                attack.timer = attack.cooldown;
            }

        } else {
            // OUT OF RANGE - CHASE
            
            // We need to move towards target.
            // Check if we are already moving to the correct location to avoid spamming PathRequests
            bool needsPathUpdate = true;
            
            if (world->HasComponent<MovementComponent>(entity)) {
                auto& move = world->GetComponent<MovementComponent>(entity);
                if (move.isMoving && !move.waypoints.empty()) {
                    Vector2 currentDest = move.waypoints.back();
                    if (Vector2Distance(currentDest, targetPosition) < 10.0f) {
                         // We are already moving roughly to the target
                         needsPathUpdate = false;
                    }
                }
            }

            if (needsPathUpdate) {
                // Issue path request
                if (world->HasComponent<PathRequestComponent>(entity)) {
                    world->GetComponent<PathRequestComponent>(entity).target = targetPosition;
                } else {
                    world->AddComponent(entity, PathRequestComponent(targetPosition));
                }
            }
        }
    });
}

void AttackSystem::SpawnProjectile(Entity attacker, const Vector2& startPos, Entity target, float damage) {
    Entity projectile = world->CreateEntity();
    
    // Visuals
    world->AddComponent(projectile, PositionComponent(startPos.x, startPos.y));
    world->AddComponent(projectile, CircleComponent(5.0f, RED, MAROON, "World")); // Simple projectile visual
    
    // Logic
    world->AddComponent(projectile, ProjectileComponent(damage, target, 400.0f)); // 400 speed
    
    // Optional: Add trail or particle effect later
    
    LOG_INFOF("AttackSystem", "Entity %zu fired projectile at %zu", attacker, target);
}

} // namespace Elysium::Systems
