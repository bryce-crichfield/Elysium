#include "Systems/MovementSystem.h"

namespace Elysium::Systems {
    void MovementSystem::Update(float deltaTime) {
        // world->ForEachEntityWith<MovementComponent, PositionComponent>([&](Entity e) {
        //     auto& movement = world->GetComponent<MovementComponent>(e);
        //     auto& pos = world->GetComponent<PositionComponent>(e);

        //     if (movement.isMoving) {
        //         // Simple lerp toward target
        //         Vector2 direction = Vector2Subtract(movement.target, pos);
        //         if (Vector2Length(direction) < 1.0f) {
        //             pos = movement.target;
        //             movement.isMoving = false;
        //         } else {
        //             pos = Vector2Add(pos, Vector2Scale(Vector2Normalize(direction), movement.speed * dt));
        //         }
        //     }
        // });
    }
}
