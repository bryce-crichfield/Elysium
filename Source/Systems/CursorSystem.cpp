#include "Systems/CursorSystem.h"
#include "Scene.h"
#include "Entity.h"

#include "raylib.h"
#include "raymath.h"

namespace Elysium::Systems {
    void CursorSystem::Update(float deltaTime) {
        if (Entity cursor; world->GetEntityByName("CURSOR", &cursor)) {
            // Check if cursor has required components
            if (!world->HasComponent<LocationComponent>(cursor) ||
                !world->HasComponent<MovementComponent>(cursor) ||
                !world->HasComponent<PositionComponent>(cursor)) {
                TraceLog(LOG_WARNING, "CURSOR entity missing required components");
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

            if (IsKeyPressed(KEY_UP)) {
                newY -= 1;
                moved = true;
                TraceLog(LOG_INFO, "Cursor UP: %d, %d", newX, newY);
            }
            if (IsKeyPressed(KEY_DOWN)) {
                newY += 1;
                moved = true;
                TraceLog(LOG_INFO, "Cursor DOWN: %d, %d", newX, newY);
            }
            if (IsKeyPressed(KEY_LEFT)) {
                newX -= 1;
                moved = true;
                TraceLog(LOG_INFO, "Cursor LEFT: %d, %d", newX, newY);
            }
            if (IsKeyPressed(KEY_RIGHT)) {
                newX += 1;
                moved = true;
                TraceLog(LOG_INFO, "Cursor RIGHT: %d, %d", newX, newY);
            }

            if (moved) {
                // Update location
                location.x = newX;
                location.y = newY;
                
                // Set up movement - use TILE coordinates (MovementSystem will convert to world)
                movement.ClearWaypoints();
                movement.AddWaypoint(Vector2{ static_cast<float>(newX), static_cast<float>(newY) });
                movement.currentWaypointIndex = 0;
                movement.isMoving = true;
                movement.loop = false;
                
                TraceLog(LOG_INFO, "Cursor movement initiated to tile (%d, %d)", newX, newY);
            }
        }
    }
};
