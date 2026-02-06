#pragma once
#include "Core/Component.h"
#include <string>

namespace Elysium {
    struct ScaleComponent {
        float x = 1.0f;
        float y = 1.0f;

        ScaleComponent() = default;
        ScaleComponent(float x, float y) : x(x), y(y) {}
        ScaleComponent(float uniform) : x(uniform), y(uniform) {}

        static constexpr const char* Name() { return "Scale"; }
        static constexpr const char* XmlTag() { return "ScaleComponent"; }

        static void LoadXml(ScaleComponent& c, tinyxml2::XMLElement* el);
        static void Inspect(ScaleComponent& c, Entity e);
        static void BindLua(sol::usertype<ScaleComponent>& ut);
        static void SetFromLua(ScaleComponent& c, sol::object v);
    };
}
