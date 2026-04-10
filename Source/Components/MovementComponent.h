#pragma once
#include "Core/Component.h"
#include "raylib.h"
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
        std::vector<Vector2> waypoints;     
        Vector2 goal;  
        int waitTimeMs;
        int stuckRetryCount;
        int stuckCheckAccumMs;
        int currentWaypointIndex;
        Vector2 lastPosition;

        static constexpr const char* Name() { return "Movement"; }
        static constexpr const char* XmlTag() { return "MovementComponent"; }

        static void LoadXml(MovementComponent& c, tinyxml2::XMLElement* el);
        static void Inspect(MovementComponent& c, Entity e);
        static void BindLua(sol::usertype<MovementComponent>& ut);
        static void SetFromLua(MovementComponent& c, sol::object v);
    };
}
