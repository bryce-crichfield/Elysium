#include "Systems/MovementSystem.h"
#include "Systems/SpatialSystem.h"
#include "Core/Component.h"
#include "Core/Entity.h"
#include "Core/Scene.h"
#include "raymath.h"
#include <algorithm>

namespace Elysium::Systems {

void MovementSystem::Update(float deltaTime) {
    if (!spatialSystem_) {
        spatialSystem_ = scene->GetSystem<SpatialSystem>();
    }

    world->Query<MovementComponent, PositionComponent>(
        [&](Entity e, auto& movement, auto& pos) {
            if (!movement.isMoving || movement.waypoints.empty())
                return;

            // Handle Waypoint Cycling
            if (movement.currentWaypointIndex >= movement.waypoints.size()) {
                if (movement.loop) {
                    movement.currentWaypointIndex = 0;
                } else {
                    movement.isMoving = false;
                    movement.waypoints.clear();
                    movement.currentWaypointIndex = 0;
                    return;
                }
            }

            Vector2 target = movement.waypoints[movement.currentWaypointIndex];
            Vector2 currentPos = {pos.x, pos.y};
            Vector2 toTarget = Vector2Subtract(target, currentPos);
            float distToTarget = Vector2Length(toTarget);

            // --- 1. Arrival Behavior (Seek) ---
            // Slow down as we approach the target
            float slowingRadius = 64.0f; // Start slowing down 64px away
            float desiredSpeed = movement.speed;
            
            if (distToTarget < slowingRadius) {
                desiredSpeed = movement.speed * (distToTarget / slowingRadius);
            }

            Vector2 desiredVelocity = {0, 0};
            if (distToTarget > 0.001f) {
                desiredVelocity = Vector2Scale(Vector2Normalize(toTarget), desiredSpeed);
            }

            // --- 2. Separation Behavior (Avoidance) ---
            Vector2 separationForce = {0, 0};
            
            if (spatialSystem_) {
                float unitRadius = 20.0f; // Default "personal space"
                
                // Use BoundsComponent for better radius estimation if available
                if (world->HasComponent<BoundsComponent>(e)) {
                    unitRadius = world->GetComponent<BoundsComponent>(e).bounds.width + 10.0f; 
                }


                float detectionRadius = unitRadius * 2.0f; // Look for neighbors within 2x radius
                auto neighbors = spatialSystem_->GetNearbyEntities(currentPos, detectionRadius);
                int count = 0;

                for (Entity other : neighbors) {
                    if (other == e) continue;

                    // Only separate from other units (things with Position)
                    // Ideally we'd check for a "Unit" tag or CollisionComponent
                    if (world->HasComponent<PositionComponent>(other)) {
                        auto& otherPos = world->GetComponent<PositionComponent>(other);
                        Vector2 toNeighbor = Vector2Subtract(currentPos, {otherPos.x, otherPos.y});
                        float dist = Vector2Length(toNeighbor);

                        // Prevent division by zero and respect radius
                        if (dist > 0.001f && dist < detectionRadius) {
                            // The closer they are, the stronger the push
                            float strength = (detectionRadius - dist) / detectionRadius;
                            
                            // Vector pointing AWAY from neighbor
                            separationForce = Vector2Add(separationForce, Vector2Scale(Vector2Normalize(toNeighbor), strength));
                            count++;
                        }
                    }
                }
                
                if (count > 0) {
                     // Average the force
                    separationForce = Vector2Scale(separationForce, 1.0f / count);
                    // Scale it up to be significant relative to movement speed
                    separationForce = Vector2Scale(separationForce, movement.speed * 2.5f); 
                }
            }

            // --- 3. Combine Forces ---
            Vector2 finalVelocity = Vector2Add(desiredVelocity, separationForce);
            
            // Limit to max speed (allow slight overspeed for separation bursts)
            if (Vector2Length(finalVelocity) > movement.speed) {
                finalVelocity = Vector2Scale(Vector2Normalize(finalVelocity), movement.speed);
            }

            // --- 4. Move & Update State ---
            
            bool reachedWaypoint = false;
            bool isFinalWaypoint = (movement.currentWaypointIndex == movement.waypoints.size() - 1) && !movement.loop;
            float acceptanceRadius = isFinalWaypoint ? 2.0f : 16.0f; // Tighter acceptance for final target

            if (distToTarget < acceptanceRadius) {
                reachedWaypoint = true;
                // Snap to exact position if it's the final point to stop jitter
                if (isFinalWaypoint) {
                    pos.x = target.x;
                    pos.y = target.y;
                    finalVelocity = {0, 0};
                }
            } else if (isFinalWaypoint && distToTarget < slowingRadius && Vector2Length(finalVelocity) < 5.0f) {
                 // We are close and stopped moving (likely blocked by others), consider it arrived
                 reachedWaypoint = true;
            }

            if (reachedWaypoint) {
                // Determine next step
                movement.currentWaypointIndex++;
                
                // If we are done, stop moving entirely
                if (movement.currentWaypointIndex >= movement.waypoints.size() && !movement.loop) {
                     movement.isMoving = false;
                     movement.waypoints.clear();
                     movement.currentWaypointIndex = 0;
                     finalVelocity = {0, 0}; 
                }
            }

            // Apply the move
            if (world->HasComponent<KinematicsComponent>(e)) {
                auto& kin = world->GetComponent<KinematicsComponent>(e);
                
                // If we are trying to stop (final velocity 0) or slow down, accelerate faster (brakes)
                float currentSpeed = Vector2Length(kin.velocity);
                float desiredSpeed = Vector2Length(finalVelocity);
                
                float accelerationFactor = 10.0f;
                if (desiredSpeed < currentSpeed) {
                    accelerationFactor = 20.0f; // Stronger braking
                }
                
                float t = std::min(accelerationFactor * deltaTime, 1.0f);
                kin.velocity.x = Lerp(kin.velocity.x, finalVelocity.x, t);
                kin.velocity.y = Lerp(kin.velocity.y, finalVelocity.y, t);
                
                // If snapped above, ensure kinematics velocity is also killed
                if (reachedWaypoint && isFinalWaypoint) {
                    kin.velocity = {0, 0};
                }
                
                // KinematicsSystem will handle the position update
            } else {
                // Fallback: direct position update
                Vector2 newPos = Vector2Add(currentPos, Vector2Scale(finalVelocity, deltaTime));
                pos.x = newPos.x;
                pos.y = newPos.y;
            }

            // --- 5. Update Facing Direction ---
            Vector2 effectiveVelocity = finalVelocity;
            if (world->HasComponent<KinematicsComponent>(e)) {
                effectiveVelocity = world->GetComponent<KinematicsComponent>(e).velocity;
            }

            if (world->HasComponent<DirectionComponent>(e) && Vector2Length(effectiveVelocity) > 1.0f) {
                auto& dirComp = world->GetComponent<DirectionComponent>(e);
                Direction newDir = Direction::NONE;
                if (abs(effectiveVelocity.x) > abs(effectiveVelocity.y)) {
                    newDir = (effectiveVelocity.x > 0) ? Direction::RIGHT : Direction::LEFT;
                } else {
                    newDir = (effectiveVelocity.y > 0) ? Direction::DOWN : Direction::UP;
                }
                dirComp.SetDirection(newDir);
            }
        });
}
}  // namespace Elysium::Systems
