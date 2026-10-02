#pragma once
#include "Core/Component.h"
#include "Core/MathTypes.h"
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

        static void LoadXml(MovementComponent& c, tinyxml2::XMLElement* el, ServiceLocator& services);
        // Persists only the authored state (state/goal); waypoints and stuck timers are runtime.
        // Without a saver the prefab/scene savers silently drop the component on save.
        static void SaveXml(const MovementComponent& c, XMLBuilder& builder);
        static void BindLua(sol::usertype<MovementComponent>& ut);
        static void SetFromLua(MovementComponent& c, sol::object v);
    };
}
