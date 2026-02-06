#pragma once
#include "Core/Component.h"
#include "raylib.h"
#include <string>

namespace Elysium {
    struct LightComponent {
        Color color;
        float radius;
        float intensity; // 0 = soft diffuse falloff, 1 = hard solid edge

        LightComponent(Color c = WHITE, float r = 50.0f, float i = 0.5f) : color(c), radius(r), intensity(i) {}

        static constexpr const char* Name() { return "Light"; }
        static constexpr const char* XmlTag() { return "LightComponent"; }

        static void LoadXml(LightComponent& c, tinyxml2::XMLElement* el);
        static void Inspect(LightComponent& c, Entity e);
        static void BindLua(sol::usertype<LightComponent>& ut);
        static void SetFromLua(LightComponent& c, sol::object v);
    };
}
