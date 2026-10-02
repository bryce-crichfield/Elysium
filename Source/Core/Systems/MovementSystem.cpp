#include "Core/Systems/MovementSystem.h"
#include "Core/SystemRegistry.h"
#include "Core/Systems/NavMeshSystem.h"
#include "Core/Component.h"
#include "Core/Entity.h"
#include "Core/Scene.h"
#include <algorithm>
#include <cmath>
#include "Core/Components/MovementComponent.h"
#include "Core/Components/TransformComponent.h"
#include "Core/Components/BoundsComponent.h"
#include "Core/Components/KinematicsComponent.h"
namespace Elysium::Systems {

static constexpr int   STUCK_CHECK_INTERVAL_MS = 1000;
static constexpr float STUCK_DIST_THRESHOLD    = 4.0f;
static constexpr int   WAIT_MIN_MS             = 100;
static constexpr int   WAIT_MAX_MS             = 400;
static constexpr int   MAX_REPLAN_ATTEMPTS      = 5;
static constexpr float WAYPOINT_ARRIVE_MIN      = 6.0f;   // px; grows with per-frame travel so we never overshoot-oscillate
static constexpr float ARRIVE_SLOWDOWN_DIST     = 24.0f;  // ease into the final goal over this distance

float MathLerp(float start, float end, float t) {
    return start + t * (end - start);
}

Vector2 Ground(const Vector3& waypoint) { return {waypoint.x, waypoint.y}; }

// Whether heading straight from (`from`, height `z`) to waypoint `to` keeps to the heights of the
// waypoints it skips (`first` up to `to`): they lie on the straight climb, within a little.
bool KeepsHeight(Vector2 from, float z, const std::vector<Vector3>& waypoints, int first, int to) {
    constexpr float kTolerance = 2.0f;
    const Vector3& end = waypoints[to];
    const float length = (Ground(end) - from).Length();
    for (int k = first; k < to; ++k) {
        const float along = (Ground(waypoints[k]) - from).Length();
        const float t = length > 0.0f ? std::min(1.0f, along / length) : 1.0f;
        if (std::fabs(waypoints[k].z - MathLerp(z, end.z, t)) > kTolerance) return false;
    }
    return true;
}

void MovementSystem::Update(float deltaTime) {
    if (!navMesh_)       navMesh_       = scene->GetSystem<NavMeshSystem>();

    // Consume the MoveCommand queue.  
    while (!moveCommands_.empty()) {
        MoveCommand cmd = moveCommands_.front();
        moveCommands_.pop();

        if (!world->HasComponent<TransformComponent>(cmd.entity) ||
            !world->HasComponent<MovementComponent>(cmd.entity)) continue;  // not a mover
        auto& transform = world->GetComponent<TransformComponent>(cmd.entity);
        auto& mv = world->GetComponent<MovementComponent>(cmd.entity);

        mv.goal = cmd.target;
        mv.state = MovementState::Moving;
        mv.waypoints.clear();  // Clear existing waypoints; GlobalSteeringSystem will replan on next update.
        mv.currentWaypointIndex = 0;
        mv.segmentStart = {transform.worldX, transform.worldY, transform.worldZ};
        mv.stuckRetryCount = 0;
        mv.stuckCheckAccumMs = 0;

        // Perform A* pathfinding and set the result to mv.waypoints.
        // NavMeshSystem (baked from prefab NavAreas + static colliders) is the pathfinding
        // authority; without one, no path.
        Vector2 from{transform.worldX, transform.worldY};
        std::vector<Vector3> result;
        if (navMesh_)            result = navMesh_->FindPath(from, cmd.target, transform.worldZ);
        mv.waypoints = std::move(result);

        // If no path found, consider going Idle or just setting goal directly.
        if (mv.waypoints.empty()) {
            mv.state = MovementState::Idle;
            mv.goal = cmd.target;
        }
    }

    world->Query<TransformComponent, KinematicsComponent, MovementComponent>(
        [&](Entity e, auto& transform, auto& kin, auto& mv) {

            Vector2 currentPos = {transform.worldX, transform.worldY};

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
                    mv.segmentStart = {transform.worldX, transform.worldY, transform.worldZ};
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
                float traveled = (currentPos - mv.lastPosition).Length();
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
            // Pop waypoints we've reached. The radius scales with how far we move per frame,
            // otherwise a fast unit overshoots a 2px target and oscillates around it.
            const int previousIndex = mv.currentWaypointIndex;
            const float arriveDist = std::max(WAYPOINT_ARRIVE_MIN, kin.maxSpeed * deltaTime * 1.5f);
            while (mv.currentWaypointIndex < (int)mv.waypoints.size()) {
                Vector2 wp = Ground(mv.waypoints[mv.currentWaypointIndex]);
                float dist = (currentPos - wp).Length();
                if (dist < arriveDist) {
                    mv.currentWaypointIndex++;
                } else {
                    break;
                }
            }
            // Look ahead: if we can already see a later waypoint, steer for it. This re-pulls
            // the string from where the unit actually is, so corners are rounded smoothly instead
            // of walked to the exact cell centre and turned at. Never past a bend in the height,
            // though: the climb between waypoints is lerped, so skipping the foot of the stairs
            // would start the climb early.
            if (navMesh_) {
                for (int i = (int)mv.waypoints.size() - 1; i > mv.currentWaypointIndex; --i) {
                    if (!KeepsHeight(currentPos, transform.worldZ, mv.waypoints, mv.currentWaypointIndex, i)) continue;
                    if (navMesh_->HasLineOfSight(currentPos, Ground(mv.waypoints[i]), transform.worldZ)) { mv.currentWaypointIndex = i; break; }
                }
            }
            if (mv.currentWaypointIndex != previousIndex) {
                mv.segmentStart = {currentPos.x, currentPos.y, transform.worldZ};
            }

            // All waypoints consumed — arrived at goal.
            if (mv.currentWaypointIndex >= (int)mv.waypoints.size()) {
                // transform.localX = mv.goal.x;
                // transform.localY = mv.goal.y;
                mv.state = MovementState::Idle;
                mv.waypoints.clear();
                mv.currentWaypointIndex = 0;
                mv.stuckRetryCount = 0;
                kin.velocity = {0, 0};
                return;
            }

            // Steer toward current waypoint, easing in on the final one so we settle rather than overshoot.
            Vector2 wp = Ground(mv.waypoints[mv.currentWaypointIndex]);
            Vector2 toWp = wp - currentPos;
            Vector2 dir = toWp.Normalized();
            float speed = kin.maxSpeed;
            const bool lastWaypoint = mv.currentWaypointIndex == (int)mv.waypoints.size() - 1;
            if (lastWaypoint && toWp.Length() < ARRIVE_SLOWDOWN_DIST) {
                speed = kin.maxSpeed * std::max(0.25f, toWp.Length() / ARRIVE_SLOWDOWN_DIST);
            }
            kin.velocity = dir * speed;
        }
    );

    // Follow behaviour is handled by FollowSystem (uses ParentComponent as target).
}
}  // namespace Elysium::Systems

REGISTER_SYSTEM(Elysium::Systems::MovementSystem)
