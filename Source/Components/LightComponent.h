#pragma once
#include "Core/Component.h"
#include "raylib.h"
#include <string>

namespace Elysium {
    struct LightComponent {
        Color color;
        float radius;
        std::string layerName = "default";

        LightComponent(Color c = WHITE, float r = 50.0f, const std::string& layer = "default") : color(c), radius(r), layerName(layer) {}

        static constexpr const char* Name() { return "Light"; }
        static constexpr const char* XmlTag() { return "LightComponent"; }

        static void LoadXml(LightComponent& c, tinyxml2::XMLElement* el);
        static void Inspect(LightComponent& c, Entity e);
        static void BindLua(sol::usertype<LightComponent>& ut);
        static void SetFromLua(LightComponent& c, sol::object v);
    };
}
