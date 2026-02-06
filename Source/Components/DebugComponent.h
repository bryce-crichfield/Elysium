#pragma once
#include "Core/Component.h"
#include "raylib.h"

namespace Elysium {
    struct DebugComponent {
        bool isSelected = false;
        Color highlightColor = GREEN;

        Vector2 aabbMin = {0.0f, 0.0f};
        Vector2 aabbMax = {0.0f, 0.0f};

        DebugComponent() = default;

        static constexpr const char* Name() { return "Debug"; }
        static constexpr const char* XmlTag() { return "DebugComponent"; }

        static void LoadXml(DebugComponent& c, tinyxml2::XMLElement* el);
        static void Inspect(DebugComponent& c, Entity e);
        static void BindLua(sol::usertype<DebugComponent>& ut);
        static void SetFromLua(DebugComponent& c, sol::object v);
    };
}
