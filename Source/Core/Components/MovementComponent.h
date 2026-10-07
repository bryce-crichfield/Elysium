#pragma once
#include "Core/Component.h"
#include "Core/Math/MathTypes.h"
#include <optional>
#include <vector>

namespace Elysium {
    // The pathfinding system operates at two layers. 
    // GlobalSteeringSystem routes entities along the tilemap.
    // LocalSteeringSystem handles local movement and obstacle avoidance.
    // Collision
    enum class MovementState {
        Idle,
        Moving,
        Waiting
    };

    struct MovementComponent {
        MovementState state;
        // Ground position (x, y) and the height of the floor there (z).
        std::vector<Vector3> waypoints;
        Vector2 goal;  
        int waitTimeMs;
        int stuckRetryCount;
        int stuckCheckAccumMs;
        int currentWaypointIndex;
        Vector2 lastPosition;
        // Where the unit was (ground position and height) when it set off for the current
        // waypoint; its height on the way is lerped from here to the waypoint's.
        Vector3 segmentStart;

        // The path's height at ground position `pos`, while following one; else nothing.
        std::optional<float> PathHeight(Vector2 pos) const;

        static constexpr const char* Name() { return "Movement"; }
        static constexpr const char* XmlTag() { return "MovementComponent"; }

        static void LoadXml(MovementComponent& c, tinyxml2::XMLElement* el, ServiceLocator& services);
        // Persists only the authored state (state/goal); waypoints and stuck timers are runtime.
        // Without a saver the prefab/scene savers silently drop the component on save.
        static void SaveXml(const MovementComponent& c, XMLBuilder& builder);
        static void BindLua(sol::usertype<MovementComponent>& ut);
        static void SetFromLua(MovementComponent& c, sol::object v);
    };
}
