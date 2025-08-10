#include "Systems/PhysicsSystem.h"

#include "Application.h"
#include "Scene.h"
#include "Entity.h"

#include "raylib.h"

namespace Elysium::Systems {

void PhysicsSystem::Update(float deltaTime) {
    const float screenWidth = (float)GetScreenWidth();
    const float screenHeight = (float)GetScreenHeight();

    // Use iterator-based approach to avoid temporary vector allocation
    world->ForEachEntityWith<PositionComponent, VelocityComponent, PhysicsComponent>([&](Entity entity) {
        auto& pos = world->GetComponent<PositionComponent>(entity);
        auto& vel = world->GetComponent<VelocityComponent>(entity);
        auto& physics = world->GetComponent<PhysicsComponent>(entity);

        // Apply gravity
        if (physics.affectedByGravity) {
            vel.y += gravity_ * deltaTime;
        }

        // Update position
        pos.x += vel.x * deltaTime;
        pos.y += vel.y * deltaTime;

        // Get radius for collision detection if it has a circle render component
        float radius = 10.0f; // default
        if (world->HasComponent<CircleComponent>(entity)) {
            radius = world->GetComponent<CircleComponent>(entity).radius;
        }

        // Bounce off walls
        if (pos.x <= radius || pos.x >= screenWidth - radius) {
            vel.x *= -physics.restitution;
            pos.x = (pos.x <= radius) ? radius : screenWidth - radius;
        }

        if (pos.y <= radius || pos.y >= screenHeight - radius) {
            vel.y *= -physics.restitution;
            pos.y = (pos.y <= radius) ? radius : screenHeight - radius;
        }
    });
}

} // namespace Elysium::Systems
