#pragma once
#include "Core/Component.h"
#include "raylib.h"
#include <vector>

namespace Elysium {
    struct MovementComponent {
        std::vector<Vector2> waypoints;
        size_t currentWaypointIndex = 0;
        float speed = 100.0f;
        bool isMoving = false;
        bool loop = true;  // should it loop back to start when done?

        void AddWaypoint(const Vector2& waypoint);
        void ClearWaypoints();

        MovementComponent() = default;
        MovementComponent(const std::vector<Vector2>& waypoints);

        static constexpr const char* Name() { return "Movement"; }
        static constexpr const char* XmlTag() { return "MovementComponent"; }

        static void LoadXml(MovementComponent& c, tinyxml2::XMLElement* el);
        static void Inspect(MovementComponent& c, Entity e);
        static void BindLua(sol::usertype<MovementComponent>& ut);
        static void SetFromLua(MovementComponent& c, sol::object v);
    };
}
