#include "Systems/CursorSystem.h"
#include "Components/LocationComponent.h"
#include "Components/MovementComponent.h"
#include "Components/PositionComponent.h"
#include "Services/LogService.h"

// TODO: these should come from the tilemap/scene config
constexpr int MAP_WIDTH = 100;
constexpr int MAP_HEIGHT = 100;

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

        auto& location = world->GetComponent<LocationComponent>(cursor);
        auto& movement = world->GetComponent<MovementComponent>(cursor);
        auto& position = world->GetComponent<PositionComponent>(cursor);

        // Only accept input when not currently moving
        if (movement.isMoving) {
            return;
        }

        bool moved = false;
        int newX = location.x;
        int newY = location.y;

        if (IsKeyDown(KEY_UP) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_UP)) {
            newY -= 1;
            moved = true;
        } else if (IsKeyDown(KEY_DOWN) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_DOWN)) {
            newY += 1;
            moved = true;
        } else if (IsKeyDown(KEY_LEFT) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_LEFT)) {
            newX -= 1;
            moved = true;
        } else if (IsKeyDown(KEY_RIGHT) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_RIGHT)) {
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
            movement.AddWaypoint(Vector2{static_cast<float>(newX), static_cast<float>(newY)});
            movement.currentWaypointIndex = 0;
            movement.isMoving = true;
            movement.loop = false;
        }
    }
}
};  // namespace Elysium::Systems
