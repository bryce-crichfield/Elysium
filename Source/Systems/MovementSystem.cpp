#include "Systems/MovementSystem.h"
#include "Component.h"
#include "Entity.h"
#include "raymath.h"
#include "Scene.h"

namespace Elysium::Systems {
    void MovementSystem::Update(float deltaTime) {

        world->Query<MovementComponent, PositionComponent>(
            [&](Entity e, auto& movement, auto& pos) {

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

                // Set direction to NONE when stopped (if DirectionComponent exists)
                if (world->HasComponent<DirectionComponent>(e)) {
                    auto& dirComp = world->GetComponent<DirectionComponent>(e);
                    dirComp.SetDirection(Direction::NONE);
                }
            } else {
                // Move toward current waypoint
                Vector2 velocity = Vector2Scale(Vector2Normalize(direction), movement.speed * deltaTime);
                Vector2 newPos = Vector2Add({pos.x, pos.y}, velocity);
                pos.x = newPos.x;
                pos.y = newPos.y;

                // Update direction component if it exists
                if (world->HasComponent<DirectionComponent>(e)) {
                    auto& dirComp = world->GetComponent<DirectionComponent>(e);

                    // Determine direction based on movement vector
                    Direction newDir = Direction::NONE;
                    if (abs(direction.x) > abs(direction.y)) {
                        newDir = (direction.x > 0) ? Direction::RIGHT : Direction::LEFT;
                    } else {
                        newDir = (direction.y > 0) ? Direction::DOWN : Direction::UP;
                    }

                    dirComp.SetDirection(newDir);
                }
            }
        });
    }
}
