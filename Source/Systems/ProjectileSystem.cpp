#include "Systems/ProjectileSystem.h"
#include "Core/Component.h"
#include "Core/Entity.h"
#include "Core/Scene.h"
#include "Services/LogService.h"
#include "raymath.h"
#include "Components/ProjectileComponent.h"
#include "Components/PositionComponent.h"
#include "Components/HealthComponent.h"

namespace Elysium::Systems {

ProjectileSystem::ProjectileSystem(Context context) : System(context) {
}

void ProjectileSystem::Update(float deltaTime) {
    std::vector<Entity> destroyedProjectiles;

    world->Query<ProjectileComponent, PositionComponent>([&](Entity entity, auto& proj, auto& pos) {
        // Lifetime check
        proj.lifetime -= deltaTime;
        if (proj.lifetime <= 0.0f) {
            destroyedProjectiles.push_back(entity);
            return;
        }

        // Target validation
        if (!world->HasComponent<PositionComponent>(proj.targetId)) {
            // Target lost/destroyed -> Destroy projectile (or make it dumb fire? For now destroy)
            destroyedProjectiles.push_back(entity);
            return;
        }

        auto& targetPos = world->GetComponent<PositionComponent>(proj.targetId);
        Vector2 currentPos = {pos.x, pos.y};
        Vector2 destPos = {targetPos.x, targetPos.y};
        
        // Move towards target
        Vector2 dir = Vector2Subtract(destPos, currentPos);
        float dist = Vector2Length(dir);
        
        if (dist < 5.0f) {
            // HIT!
            if (world->HasComponent<HealthComponent>(proj.targetId)) {
                auto& health = world->GetComponent<HealthComponent>(proj.targetId);
                health.current -= proj.damage;
                LOG_INFOF("ProjectileSystem", "Hit target %zu for %.1f damage. Health: %.1f/%.1f", 
                          proj.targetId, proj.damage, health.current, health.max);
                
                // Death check could happen here or in a separate HealthSystem
                if (health.current <= 0) {
                     // For now, let's just log it. A HealthSystem should handle destruction.
                     LOG_INFOF("ProjectileSystem", "Target %zu destroyed!", proj.targetId);
                     world->DestroyEntity(proj.targetId);
                }
            }
            
            destroyedProjectiles.push_back(entity);
        } else {
            // Move
            Vector2 velocity = Vector2Scale(Vector2Normalize(dir), proj.speed * deltaTime);
            pos.x += velocity.x;
            pos.y += velocity.y;
        }
    });

    for (Entity e : destroyedProjectiles) {
        world->DestroyEntity(e);
    }
}

} // namespace Elysium::Systems
