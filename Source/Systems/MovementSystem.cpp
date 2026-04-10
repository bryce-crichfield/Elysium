#include "Systems/MovementSystem.h"
#include "Core/SystemRegistry.h"
#include "Systems/SpatialSystem.h"
#include "Core/Component.h"
#include "Core/Entity.h"
#include "Core/Scene.h"
#include "raymath.h"
#include <algorithm>
#include "Components/MovementComponent.h"
#include "Components/PositionComponent.h"
#include "Components/BoundsComponent.h"
#include "Components/KinematicsComponent.h"
#include "Components/FollowComponent.h"
namespace Elysium::Systems {

static constexpr int   STUCK_CHECK_INTERVAL_MS = 1000;
static constexpr float STUCK_DIST_THRESHOLD    = 4.0f;
static constexpr int   WAIT_MIN_MS             = 100;
static constexpr int   WAIT_MAX_MS             = 400;
static constexpr int   MAX_REPLAN_ATTEMPTS      = 5;
static constexpr float WAYPOINT_ARRIVE_DIST     = 2.0f;

float MathLerp(float start, float end, float t) {
    return start + t * (end - start);
}

void MovementSystem::Update(float deltaTime) {
    if (!spatialSystem_) {
        spatialSystem_ = scene->GetSystem<SpatialSystem>();
    }

    // Consume the MoveCommand queue.  
    while (!moveCommands_.empty()) {
        MoveCommand cmd = moveCommands_.front();
        moveCommands_.pop();

        auto& pos = world->GetComponent<PositionComponent>(cmd.entity);
        auto& mv = world->GetComponent<MovementComponent>(cmd.entity);

        mv.goal = cmd.target;
        mv.state = MovementState::Moving;
        mv.waypoints.clear();  // Clear existing waypoints; GlobalSteeringSystem will replan on next update.
        mv.currentWaypointIndex = 0;
        mv.stuckRetryCount = 0;
        mv.stuckCheckAccumMs = 0;

        // Perform A* pathfinding and set the result to mv.waypoints. 
        std::vector<Vector2> result = spatialSystem_->FindPath(Vector2{pos.x, pos.y}, cmd.target);
        mv.waypoints = std::move(result);

        // If no path found, consider going Idle or just setting goal directly.
        if (mv.waypoints.empty()) {
            mv.state = MovementState::Idle;
            mv.goal = cmd.target;
        }
    }

    world->Query<PositionComponent, KinematicsComponent, MovementComponent>(
        [&](Entity e, auto& pos, auto& kin, auto& mv) {

            Vector2 currentPos = {pos.x, pos.y};

            if (mv.state == MovementState::Idle) {
                kin.velocity = {0, 0};
                return;
            }

            // --- WAITING: count down jitter timer, then trigger replan ---
            if (mv.state == MovementState::Waiting) {
                kin.velocity = {0, 0};
                mv.waitTimeMs -= static_cast<int>(deltaTime * 1000);
                if (mv.waitTimeMs <= 0) {
                    // Signal to pathfinding system that this entity needs a replan.
                    // Don't run A* here — that's GlobalSteeringSystem's job.
                    // Just flip back to Moving; GlobalSteeringSystem will detect
                    // that waypoints are empty and replan.
                    mv.waypoints.clear();
                    mv.currentWaypointIndex = 0;
                    mv.state = MovementState::Moving;
                    mv.stuckRetryCount++;
                }
                return;
            }

            // --- MOVING ---

            // Stuck detection: compare position to lastPosition every N ms.
            // You'll want a dedicated accumulator for this rather than checking
            // every frame — add stuckCheckAccumMs to MovementComponent.
            mv.stuckCheckAccumMs += static_cast<int>(deltaTime * 1000);
            if (mv.stuckCheckAccumMs >= STUCK_CHECK_INTERVAL_MS) {
                float traveled = Vector2Distance(currentPos, mv.lastPosition);
                if (traveled < STUCK_DIST_THRESHOLD) {
                    // Stuck — enter waiting state with random jitter.
                    // Jitter is critical: without it, two mutually blocking units
                    // replan on the same frame and deadlock again.
                    mv.state = MovementState::Waiting;
                    mv.waitTimeMs = WAIT_MIN_MS + (rand() % (WAIT_MAX_MS - WAIT_MIN_MS));
                    kin.velocity = {0, 0};
                    mv.stuckCheckAccumMs = 0;
                    mv.lastPosition = currentPos;
                    return;
                }
                mv.stuckCheckAccumMs = 0;
                mv.lastPosition = currentPos;
            }

            // Give-up check: too many replan attempts → go idle.
            if (mv.stuckRetryCount > MAX_REPLAN_ATTEMPTS) {
                mv.state = MovementState::Idle;
                mv.waypoints.clear();
                mv.stuckRetryCount = 0;
                kin.velocity = {0, 0};
                return;
            }

            // No waypoints: either just issued command or just replanned.
            // Delegate to GlobalSteeringSystem which runs before this system
            // and fills waypoints when it sees Moving + empty waypoints.
            if (mv.waypoints.empty()) {
                kin.velocity = {0, 0};
                return;
            }

            // --- WAYPOINT FOLLOWING ---
            // Pop waypoints we've reached.
            while (mv.currentWaypointIndex < (int)mv.waypoints.size()) {
                Vector2 wp = mv.waypoints[mv.currentWaypointIndex];
                float dist = Vector2Distance(currentPos, wp);
                if (dist < WAYPOINT_ARRIVE_DIST) {
                    mv.currentWaypointIndex++;
                } else {
                    break;
                }
            }

            // All waypoints consumed — arrived at goal.
            if (mv.currentWaypointIndex >= (int)mv.waypoints.size()) {
                // pos.x = mv.goal.x;
                // pos.y = mv.goal.y;
                mv.state = MovementState::Idle;
                mv.waypoints.clear();
                mv.currentWaypointIndex = 0;
                mv.stuckRetryCount = 0;
                kin.velocity = {0, 0};
                return;
            }

            // Steer toward current waypoint.
            Vector2 wp = mv.waypoints[mv.currentWaypointIndex];
            Vector2 dir = Vector2Subtract(wp, currentPos);
            dir = Vector2Normalize(dir);
            kin.velocity = Vector2Scale(dir, kin.maxSpeed);
        }
    );

    world->Query<PositionComponent, FollowComponent>(
        [&](Entity e, auto& pos, auto& follow) {
            Entity targetEntity;
            if (!world->GetEntityByName(follow.targetEntityName, &targetEntity)) {
                return;
            }

            if (!world->HasComponent<PositionComponent>(targetEntity)) {
                return;
            }

            auto& targetPos = world->GetComponent<PositionComponent>(targetEntity);
            pos.x = MathLerp(pos.x, targetPos.x, follow.speed * deltaTime);
            pos.y = MathLerp(pos.y, targetPos.y, follow.speed * deltaTime);
        }
    );
}
}  // namespace Elysium::Systems

REGISTER_SYSTEM(Elysium::Systems::MovementSystem)
