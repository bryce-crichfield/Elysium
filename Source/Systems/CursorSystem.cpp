#include "Systems/CursorSystem.h"
#include "Services/LogService.h"
#include "Scene.h"
#include "Entity.h"

#include "raylib.h"
#include "raymath.h"

#define MAP_WIDTH 8
#define MAP_HEIGHT 8

namespace Elysium::Systems {
    void CursorSystem::Update(float deltaTime) {
        if (Entity cursor; world->GetEntityByName("CURSOR", &cursor)) {
            // Check if cursor has required components
            if (!world->HasComponent<LocationComponent>(cursor) ||
                !world->HasComponent<MovementComponent>(cursor) ||
                !world->HasComponent<PositionComponent>(cursor)) {
                LOG_WARNING("CursorSystem", "CURSOR entity missing required components");
                return;
            }

            auto &location = world->GetComponent<LocationComponent>(cursor);
            auto& movement = world->GetComponent<MovementComponent>(cursor);
            auto& position = world->GetComponent<PositionComponent>(cursor);

            // Only accept input when not currently moving
            if (movement.isMoving) {
                return;
            }

            bool moved = false;
            int newX = location.x;
            int newY = location.y;

            if (IsKeyDown(KEY_UP)) {
                newY -= 1;
                moved = true;
            }
            else if (IsKeyDown(KEY_DOWN)) {
                newY += 1;
                moved = true;
            }
            else if (IsKeyDown(KEY_LEFT)) {
                newX -= 1;
                moved = true;
            }
            else if (IsKeyDown(KEY_RIGHT)) {
                newX += 1;
                moved = true;
            }



            // Check map boundaries (assuming map size constants are defined)
            if (moved && newX >= 0 && newX < MAP_WIDTH && newY >= 0 && newY < MAP_HEIGHT) {
                // Update location
                location.x = newX;
                location.y = newY;

                // Set up movement - use TILE coordinates (MovementSystem will convert to world)
                movement.ClearWaypoints();
                movement.AddWaypoint(Vector2{ static_cast<float>(newX), static_cast<float>(newY) });
                movement.currentWaypointIndex = 0;
                movement.isMoving = true;
                movement.loop = false;
            }
        }
    }
};
