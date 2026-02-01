#pragma once
#include "Core/Component.h"
#include "raylib.h"

namespace Elysium {
    struct PathRequestComponent {
        Vector2 target;
        PathRequestComponent() : target({0.0f, 0.0f}) {}
        PathRequestComponent(Vector2 t) : target(t) {}

        static constexpr const char* Name() { return "PathRequest"; }
        static constexpr const char* XmlTag() { return "PathRequestComponent"; }

        static void LoadXml(PathRequestComponent& c, tinyxml2::XMLElement* el);
        static void Inspect(PathRequestComponent& c, Entity e);
        static void BindLua(sol::usertype<PathRequestComponent>& ut);
        static void SetFromLua(PathRequestComponent& c, sol::object v);
    };
}
