#include "Systems/KinematicsSystem.h"
#include "Core/Component.h"
#include "Core/Entity.h"
#include "Core/Scene.h"
#include "raymath.h"

namespace Elysium::Systems {

void KinematicsSystem::Update(float deltaTime) {
    world->Query<KinematicsComponent, PositionComponent>(
        [&](Entity e, auto& kin, auto& pos) {
            // 1. Integrate Acceleration into Velocity
            // v = v + a * dt
            if (kin.acceleration.x != 0 || kin.acceleration.y != 0) {
                kin.velocity.x += kin.acceleration.x * deltaTime;
                kin.velocity.y += kin.acceleration.y * deltaTime;
            }

            // 2. Apply Friction/Damping
            // Friction applies opposite to velocity
            // Simple damping: v = v * (1 - friction * dt)
            // Or linear drag: v = v - friction * v * dt
            if (kin.friction > 0) {
                float speed = Vector2Length(kin.velocity);
                if (speed > 0) {
                    float drop = speed * kin.friction * deltaTime;
                    float newSpeed = speed - drop;
                    if (newSpeed < 0) newSpeed = 0;
                    
                    if (speed > 0.0001f) {
                         kin.velocity = Vector2Scale(kin.velocity, newSpeed / speed);
                    } else {
                        kin.velocity = {0,0};
                    }
                }
            }
            
            // 3. Clamp to MaxSpeed
            if (kin.maxSpeed > 0) {
                float speed = Vector2Length(kin.velocity);
                if (speed > kin.maxSpeed) {
                    kin.velocity = Vector2Scale(kin.velocity, kin.maxSpeed / speed);
                }
            }

            // 4. Integrate Velocity into Position
            // p = p + v * dt
            // Only update if moving noticeably
            if (Vector2LengthSqr(kin.velocity) > 0.001f) {
                pos.x += kin.velocity.x * deltaTime;
                pos.y += kin.velocity.y * deltaTime;
            }

            // Reset acceleration for next frame (force accumulation style)
            kin.acceleration = {0, 0};
        });
}

}  // namespace Elysium::Systems
