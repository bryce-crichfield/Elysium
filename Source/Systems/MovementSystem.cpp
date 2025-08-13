#include "Systems/MovementSystem.h"
#include "Component.h"
#include "Entity.h"
#include "raymath.h"
#include "Scene.h"

namespace Elysium::Systems {
    void MovementSystem::Update(float deltaTime) {

        world->ForEachEntityWith<MovementComponent, PositionComponent>([&](Entity e) {
            auto& movement = world->GetComponent<MovementComponent>(e);
            auto& pos = world->GetComponent<PositionComponent>(e);

            if (!movement.isMoving || movement.waypoints.empty()) return;

            // Get current target waypoint
            if (movement.currentWaypointIndex >= movement.waypoints.size()) {
                if (movement.loop) {
                    movement.currentWaypointIndex = 0;
                } else {
                    movement.isMoving = false;
                    return;
                }
            }

            Vector2 target = movement.waypoints[movement.currentWaypointIndex];
            target.x *= TILE_WIDTH;
            target.x += TILE_WIDTH / 2;
            target.y *= TILE_HEIGHT;
            target.y += TILE_HEIGHT / 2;
            Vector2 direction = Vector2Subtract(target, {pos.x, pos.y});
            float distance = Vector2Length(direction);

            if (distance < 1.0f) {
                // Reached waypoint, move to next
                pos.x = target.x;
                pos.y = target.y;
                movement.currentWaypointIndex++;
            } else {
                // Move toward current waypoint
                Vector2 velocity = Vector2Scale(Vector2Normalize(direction), movement.speed * deltaTime);
                Vector2 newPos = Vector2Add({pos.x, pos.y}, velocity);
                pos.x = newPos.x;
                pos.y = newPos.y;
            }
        });
    }
}
